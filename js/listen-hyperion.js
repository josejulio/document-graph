const HyperionSocketClient = require('@eosrio/hyperion-stream-client').default;
const dgraph = require("dgraph-js");
const grpc = require("grpc");
const { Api, JsonRpc } = require("eosjs");
const fetch = require("node-fetch");

async function getDocument(host, contract, dochash) {
    let rpc;
    let options = {};

    rpc = new JsonRpc(host, { fetch });
    options.code = contract;
    options.json = true;
    options.scope = contract;
    options.table = "documents";
    options.index_position = 2;
    options.key_type = "sha256";
    // options.encode_type = "hex";
    options.upper_bound = dochash;
    options.lower_bound = dochash;
    options.limit = 1;

    const result = await rpc.get_table_rows(options);
    if (result.rows.length > 0) {
        return result.rows[0];
    }

    console.log("There is no document with hash: ", dochash);
    return undefined;
}


const clientStub = new dgraph.DgraphClientStub(
    // addr: optional, default: "localhost:9080"
    "localhost:9080",
    // credentials: optional, default: grpc.credentials.createInsecure()
    grpc.credentials.createInsecure(),
  );
  const dgraphClient = new dgraph.DgraphClient(clientStub);

const ENDPOINT = "https://testnet.telos.caleos.io";
const client = new HyperionSocketClient(ENDPOINT, { async: false });

client.onConnect = () => {
    client.streamActions({
        contract: 'docs.hypha',
        action: 'created',
        account: 'docs.hypha',
        start_from: '2020-08-15T00:00:00.000Z',
        read_until: 0,
        filters: [],
    });
}

// see 3 for handling data
client.onData = async (data, ack) => {
    console.log ("A new document has been created:", data.content.act.data.hash);
    console.log ("Document:");
    const document = await getDocument("https://test.telos.kitchen", "docs.hypha", data.content.act.data.hash);
    console.log(JSON.stringify(document, null, 2)); // process incoming data, replace with your code    
}

client.connect(() => {
    console.log('connected!');
});