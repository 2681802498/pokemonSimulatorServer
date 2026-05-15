package protocol

import (
	"encoding/json"
	"go-server/internal/model"
)

// 错误码定义
const (
	CodeSuccess   = 0
	CodeServerErr = 10001
	CodeParamErr  = 10002

	CodeSendInvalid   = 20001
	CodeSendDuplicate = 20002
	CodeDataInvalid   = 20003

	CodeCppRPCError = 30001

	CodeRoomNotExist    = 40001
	CodeRoomFull        = 40002
	CodeRoomStarted     = 40003
	CodeRoomFinished    = 40004
	CodePlayerNotInRoom = 40005

	CodePlayerConnectedFailed   = 50001
	CodePlayerReconnectedFailed = 50002
)

// 前端发来的基础结构
type GameRequest struct {
	Cmd  string          `json:"cmd"`
	Data json.RawMessage `json:"data"`
}

// 返回给前端的统一结构
type GameResponse struct {
	Cmd  string      `json:"cmd"`
	Code int         `json:"code"`
	Msg  string      `json:"msg"`
	Data interface{} `json:"data"`
}

type ReconnectResponse struct {
	RoomID    string         `json:"room_id"`
	RoomState int            `json:"room_status"`
	Players   []model.Player `json:"players"`
	GameData  interface{}    `json:"game_data"`
}

// Pokemon 宝可梦信息
type Pokemon struct {
	SpeciesID int          `json:"species_id"`
	Level     int          `json:"level"`
	Nature    int          `json:"nature"`
	Ability   any          `json:"ability"`
	Item      int          `json:"item"`
	IVs       PokemonStats `json:"ivs"`
	EVs       PokemonStats `json:"evs"`
	Moves     []int        `json:"moves"`
}

// PokemonStats 宝可梦个体值/努力值
type PokemonStats struct {
	HP             int `json:"hp"`
	Attack         int `json:"attack"`
	Defense        int `json:"defense"`
	SpecialAttack  int `json:"specialAttack"`
	SpecialDefense int `json:"specialDefense"`
	Speed          int `json:"speed"`
}

// SelectPokemonRequest 前端发送的宝可梦选择请求
type SelectPokemonRequest struct {
	Pokemon []Pokemon `json:"pokemon"`
}
