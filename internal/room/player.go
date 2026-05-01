package room

import (
	"go-server/configs"
)

var MatchQueue = make(chan *Session, configs.MatchQueueSize)
