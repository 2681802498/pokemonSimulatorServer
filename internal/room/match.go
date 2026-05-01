package room

import "log"

func (rm *RoomManager) StartMatchWorker() {
	log.Println("匹配工作协程已启动")

	go func() {
		for {
			player1 := <-MatchQueue
			log.Printf("玩家 [%s] 进入匹配队列", player1.Player.ID)

			player2 := <-MatchQueue
			log.Printf("匹配成功: [%s] vs [%s]", player1.Player.ID, player2.Player.ID)

			rm.CreateRoom(player1)
			rm.JoinRoom(player1.RoomID, player2)

		}
	}()
}
