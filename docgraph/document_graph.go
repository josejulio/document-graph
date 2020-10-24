package docgraph

import (
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"

	eostest "github.com/digital-scarcity/eos-go-test"
	eos "github.com/eoscanada/eos-go"
)

// DocumentGraph is defined by a root node, and is aware of nodes and edges
type DocumentGraph struct {
	RootNode Document
}

type createDoc struct {
	Creator       eos.AccountName `json:"creator"`
	ContentGroups []ContentGroup  `json:"content_groups"`
}

// CreateDocument creates a new document on chain from the provided file
func CreateDocument(ctx context.Context, api *eos.API,
	contract, creator eos.AccountName,
	fileName string) (Document, error) {

	data, err := ioutil.ReadFile(fileName)
	if err != nil {
		return Document{}, fmt.Errorf("readfile %v: %v", fileName, err)
	}

	action := eostest.ToActionName("create", "action")

	var dump map[string]interface{}
	err = json.Unmarshal(data, &dump)
	if err != nil {
		return Document{}, fmt.Errorf("unmarshal %v: %v", fileName, err)
	}

	dump["creator"] = creator

	actionBinary, err := api.ABIJSONToBin(ctx, contract, eos.Name(action), dump)
	if err != nil {
		return Document{}, fmt.Errorf("api json to bin %v: %v", fileName, err)
	}

	actions := []*eos.Action{
		{
			Account: contract,
			Name:    action,
			Authorization: []eos.PermissionLevel{
				{Actor: creator, Permission: eos.PN("active")},
			},
			ActionData: eos.NewActionDataFromHexData([]byte(actionBinary)),
		}}

	_, err = eostest.ExecTrx(ctx, api, actions)
	if err != nil {
		return Document{}, fmt.Errorf("execute transaction %v: %v", fileName, err)
	}

	lastDoc, err := GetLastDocument(ctx, api, contract)
	if err != nil {
		return Document{}, fmt.Errorf("get last document %v: %v", fileName, err)
	}
	return lastDoc, nil
}

// CreateEdge creates an edge from one document node to another with the specified name
func CreateEdge(ctx context.Context, api *eos.API,
	contract, creator eos.AccountName,
	fromNode, toNode eos.Checksum256,
	edgeName eos.Name) (string, error) {

	actionData := make(map[string]interface{})
	actionData["from_node"] = fromNode
	actionData["to_node"] = toNode
	actionData["edge_name"] = edgeName

	actionBinary, err := api.ABIJSONToBin(ctx, contract, eos.Name("newedge"), actionData)
	if err != nil {
		log.Println("Error with ABIJSONToBin: ", err)
		return "error", err
	}

	actions := []*eos.Action{
		{
			Account: contract,
			Name:    eos.ActN("newedge"),
			Authorization: []eos.PermissionLevel{
				{Actor: creator, Permission: eos.PN("active")},
			},
			ActionData: eos.NewActionDataFromHexData([]byte(actionBinary)),
		}}

	return eostest.ExecTrx(ctx, api, actions)
}

func (d *Document) getEdges(ctx context.Context, api *eos.API, contract eos.AccountName, edgeIndex string) ([]Edge, error) {
	var edges []Edge
	var request eos.GetTableRowsRequest
	request.Code = string(contract)
	request.Scope = string(contract)
	request.Table = "edges"
	request.Index = edgeIndex
	request.KeyType = "sha256"
	request.LowerBound = d.Hash.String()
	request.UpperBound = d.Hash.String()
	request.Limit = 1000
	request.JSON = true
	response, err := api.GetTableRows(ctx, request)
	if err != nil {
		log.Println("Error with GetTableRows: ", err)
		return []Edge{}, err
	}

	err = response.JSONToStructs(&edges)
	if err != nil {
		log.Println("Error with JSONToStructs: ", err)
		return []Edge{}, err
	}
	return edges, nil
}

// GetEdgesFrom retrieves a list of edges from this node to other nodes
func (d *Document) GetEdgesFrom(ctx context.Context, api *eos.API, contract eos.AccountName) ([]Edge, error) {
	return d.getEdges(ctx, api, contract, string("2"))
}

// GetEdgesTo retrieves a list of edges to this node from other nodes
func (d *Document) GetEdgesTo(ctx context.Context, api *eos.API, contract eos.AccountName) ([]Edge, error) {
	return d.getEdges(ctx, api, contract, string("3"))
}

// GetEdgesFromByName retrieves a list of edges from this node to other nodes
func (d *Document) GetEdgesFromByName(ctx context.Context, api *eos.API, contract eos.AccountName, edgeName eos.Name) ([]Edge, error) {
	edges, err := d.getEdges(ctx, api, contract, string("2"))
	if err != nil {
		log.Println("Error with JSONToStructs: ", err)
		return []Edge{}, err
	}

	var namedEdges []Edge
	for _, edge := range edges {
		if edge.EdgeName == edgeName {
			namedEdges = append(namedEdges, edge)
		}
	}
	return namedEdges, nil
}

// GetEdgesToByName retrieves a list of edges from this node to other nodes
func (d *Document) GetEdgesToByName(ctx context.Context, api *eos.API, contract eos.AccountName, edgeName eos.Name) ([]Edge, error) {
	edges, err := d.getEdges(ctx, api, contract, string("3"))
	if err != nil {
		log.Println("Error with JSONToStructs: ", err)
		return []Edge{}, err
	}

	var namedEdges []Edge
	for _, edge := range edges {
		if edge.EdgeName == edgeName {
			namedEdges = append(namedEdges, edge)
		}
	}
	return namedEdges, nil
}

// GetLastDocument retrieves the last document that was created from the contract
func GetLastDocument(ctx context.Context, api *eos.API, contract eos.AccountName) (Document, error) {
	var docs []Document
	var request eos.GetTableRowsRequest
	request.Code = string(contract)
	request.Scope = string(contract)
	request.Table = "documents"
	request.Reverse = true
	request.Limit = 1
	request.JSON = true
	response, err := api.GetTableRows(ctx, request)
	if err != nil {
		log.Println("Error with GetTableRows: ", err)
		return Document{}, err
	}

	err = response.JSONToStructs(&docs)
	if err != nil {
		log.Println("Error with JSONToStructs: ", err)
		return Document{}, err
	}
	return docs[0], nil
}