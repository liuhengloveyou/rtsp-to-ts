package main

func main() {

	InitMux("gogog.ts")

	if err := InitDecoder(); err != nil {
		panic(err)
	}
	defer CloseDecoder()

	if err := initRtsp("rtsp://admin:qwer1234@172.29.251.10:554/h264/ch33/main/av_stream"); err != nil {
		panic(err)
	}
	defer rtspClient.Close()

	// wait until a fatal error
	if err := rtspClient.Wait(); err != nil {
		println("rtspClient.Wait(): ", err)
	}

}
