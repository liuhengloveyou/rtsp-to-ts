package main

import (
	"time"

	rtsp2ts "github.com/liuhengloveyou/rtsp-to-ts"
)

func main() {

	rtsp2ts.InitMux("gogog.ts")

	if err := rtsp2ts.InitDecoder(); err != nil {
		panic(err)
	}
	defer rtsp2ts.CloseDecoder()

	if err := rtsp2ts.InitRtsp("rtsp://admin:qwer1234@172.29.251.10:554/h264/ch33/main/av_stream"); err != nil {
		panic(err)
	}

	for {
		time.Sleep(1)
	}
	// wait until a fatal error
	// if err := rtspClient.Wait(); err != nil {
	// 	println("rtspClient.Wait(): ", err)
	// }

}
