#include <eosio/crypto.hpp>

#include <document_graph/document.hpp>
#include <document_graph/util.hpp>

namespace hypha
{

    Document::~Document() {}
    Document::Document() {}

    Document::Document(eosio::name contract, eosio::name creator, ContentGroups contentGroups)
        : contract{contract}, creator{creator}, content_groups{std::move(contentGroups)}
    {
        emplace();
    }

    Document::Document(eosio::name contract, eosio::name creator, ContentGroup contentGroup)
        : Document(contract, creator, rollup(contentGroup))
    {
    }

    Document::Document(eosio::name contract, eosio::name creator, Content content)
        : Document(contract, creator, rollup(content))
    {
    }

    Document::Document(eosio::name contract, eosio::name creator, const std::string &label, const Content::FlexValue &value)
        : Document(contract, creator, rollup(Content(label, value)))
    {
    }

    Document::Document(eosio::name contract, const eosio::checksum256 &_hash) : contract{contract}
    {
        document_table d_t(contract, contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(_hash);
        eosio::check(h_itr != hash_index.end(), "document not found: " + readableHash(_hash));

        id = h_itr->id;
        creator = h_itr->creator;
        created_date = h_itr->created_date;
        certificates = h_itr->certificates;
        content_groups = h_itr->content_groups;
        hashContents();

        // this should never happen, only if hash algorithm somehow changed
        eosio::check(hash == _hash, "fatal error: provided and indexed hash does not match newly generated hash");
    }

    bool Document::exists(eosio::name contract, const eosio::checksum256 &_hash)
    {
        document_table d_t(contract, contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(_hash);

        if (h_itr != hash_index.end())
        {
            return true;
        }
        return false;
    }

    void Document::emplace()
    {
        hashContents();

        document_table d_t(contract, contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(hash);

        // if this content exists already, error out and send back the hash of the existing document
        eosio::check(h_itr == hash_index.end(), "document exists already: " + readableHash(hash));

        d_t.emplace(contract, [&](auto &d) {
            id = d_t.available_primary_key();
            created_date = eosio::current_time_point();
            d = *this;
        });
    }

    Document Document::getOrNew(eosio::name _contract, eosio::name _creator, ContentGroups contentGroups)
    {
        Document document{};
        document.content_groups = contentGroups;
        document.hashContents();

        Document::document_table d_t(_contract, _contract.value);
        auto hash_index = d_t.get_index<eosio::name("idhash")>();
        auto h_itr = hash_index.find(document.hash);

        // if this content exists already, return this one
        if (h_itr != hash_index.end())
        {
            document.contract = _contract;
            document.creator = h_itr->creator;
            document.created_date = h_itr->created_date;
            document.certificates = h_itr->certificates;
            document.id = h_itr->id;
            return document;
        }

        return Document(_contract, _creator, contentGroups);
    }

    Document Document::getOrNew(eosio::name contract, eosio::name creator, ContentGroup contentGroup)
    {
        return getOrNew(contract, creator, rollup(contentGroup));
    }

    Document Document::getOrNew(eosio::name contract, eosio::name creator, Content content)
    {
        return getOrNew(contract, creator, rollup(content));
    }

    Document Document::getOrNew(eosio::name contract, eosio::name creator, const std::string &label, const Content::FlexValue &value)
    {
        return getOrNew(contract, creator, rollup(Content(label, value)));
    }

    std::pair<int64_t, ContentGroup *> Document::getGroup(const std::string &label)
    {
        for (std::size_t i = 0; i < getContentGroups().size(); ++i)
        {
            for (Content &content : getContentGroups()[i])
            {
                if (content.label == CONTENT_GROUP_LABEL)
                {
                    eosio::check(std::holds_alternative<std::string>(content.value), "fatal error: " + CONTENT_GROUP_LABEL + " must be a string");
                    if (std::get<std::string>(content.value) == label)
                    {
                        return {(int64_t)i, &getContentGroups()[i]};
                    }
                }
            }
        }
        return {-1, nullptr};
    }

    ContentGroup *Document::getGroupOrFail(const std::string &label, const std::string &error)
    {
        auto [idx, contentGroup] = getGroup(label);
        if (idx == -1)
        {
            eosio::check(false, error);
        }
        return contentGroup;
    }

    std::pair<int64_t, Content *> Document::get(const std::string &groupLabel, const std::string &contentLabel)
    {
        auto [idx, contentGroup] = getGroup(groupLabel);
        if (idx == -1)
        {
            return {-1, nullptr};
        }

        for (std::size_t i = 0; i < contentGroup->size(); ++i)
        {
            if (contentGroup->at(i).label == contentLabel)
            {
                return {(int64_t)i, &contentGroup->at(i)};
            }
        }

        return {-1, nullptr};
    }

    Content *Document::getOrFail(const std::string &groupLabel, const std::string &contentLabel, const std::string &error)
    {
        auto [idx, item] = get(groupLabel, contentLabel);
        if (idx == -1)
        {
            eosio::check(false, error);
        }
        return item;
    }

    bool Document::contentExists(const std::string &groupLabel, const std::string &contentLabel)
    {
        auto [idx, item] = get(groupLabel, contentLabel);
        if (idx == -1)
        {
            return true;
        }
        return false;
    }

    void Document::insertOrReplace(ContentGroup &contentGroup, Content &newContent)
    {
        auto is_key = [&newContent](auto &c) {
            return c.label == newContent.label;
        };
        //First let's check if key already exists
        auto content_itr = std::find_if(contentGroup.begin(), contentGroup.end(), is_key);

        if (content_itr == contentGroup.end())
        {
            contentGroup.push_back(Content{newContent.label, newContent.value});
        }
        else
        {
            content_itr->value = newContent.value;
        }
    }

    // void Document::certify(const eosio::name &certifier, const std::string &notes)
    // {
    //     // check if document is already saved??
    //     document_table d_t(m_contract, m_contract.value);
    //     auto h_itr = hash_index.find(id);
    //     eosio::check(h_itr != d_t.end(), "document not found when attemption to certify: " + readableHash(geash()));

    //     require_auth(certifier);

    //     // TODO: should a certifier be able to sign the same document fork multiple times?
    //     d_t.modify(h_itr, m_contract, [&](auto &d) {
    //         d = std::move(this);
    //         d.certificates.push_back(new_certificate(certifier, notes));
    //     });
    // }

    const void Document::hashContents()
    {
        // save/cache the hash in the member
        hash = hashContents(content_groups);
    }

    const std::string Document::toString()
    {
        return toString(content_groups);
    }

    // static version cannot cache the hash in a member
    const eosio::checksum256 Document::hashContents(ContentGroups &contentGroups)
    {
        std::string string_data = toString(contentGroups);
        return eosio::sha256(const_cast<char *>(string_data.c_str()), string_data.length());
    }

    const std::string Document::toString(ContentGroups &contentGroups)
    {
        std::string results = "[";
        bool is_first = true;

        for (ContentGroup &contentGroup : contentGroups)
        {
            if (is_first)
            {
                is_first = false;
            }
            else
            {
                results = results + ",";
            }
            results = results + toString(contentGroup);
        }

        results = results + "]";
        return results;
    }

    const std::string Document::toString(ContentGroup &contentGroup)
    {
        std::string results = "[";
        bool is_first = true;

        for (Content &content : contentGroup)
        {
            if (is_first)
            {
                is_first = false;
            }
            else
            {
                results = results + ",";
            }
            results = results + content.toString();
        }

        results = results + "]";
        return results;
    }

    ContentGroups Document::rollup(ContentGroup contentGroup)
    {
        ContentGroups contentGroups;
        contentGroups.push_back(contentGroup);
        return contentGroups;
    }

    ContentGroups Document::rollup(Content content)
    {
        ContentGroup contentGroup;
        contentGroup.push_back(content);
        return rollup(contentGroup);
    }

} // namespace hypha