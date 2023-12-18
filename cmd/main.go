package main

import (
	"time"

	rtsp2ts "github.com/liuhengloveyou/rtsp-to-ts"
)

func main() {
	rtsp2tsObj, err := rtsp2ts.NewRtspToTS("rtsp://admin:qwer1234@172.29.251.10:554/h264/ch33/main/av_stream", "gogogo.ts")
	if err != nil {
		panic(err)
	}

	rtsp2tsObj.Run()

	for {
		time.Sleep(1)
	}
	// wait until a fatal error
	// if err := rtspClient.Wait(); err != nil {
	// 	println("rtspClient.Wait(): ", err)
	// }

}
