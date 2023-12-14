package rtsp2ts

// #cgo windows CFLAGS: -DCGO_OS_WINDOWS=1
// #cgo darwin CFLAGS: -DCGO_OS_DARWIN=1
// #cgo linux CFLAGS: -DCGO_OS_LINUX=1
// #cgo CFLAGS: -I D:/dev/av/ffmpeg-6.1-full_build-shared/include
// #cgo LDFLAGS: -L D:/dev/av/ffmpeg-6.1-full_build-shared/lib -lavcodec -lavformat -lswscale -lswresample -lavutil -lavfilter
//
// #include <libavcodec/avcodec.h>
// #include <libavutil/imgutils.h>
// #include <libswscale/swscale.h>
// #include <libavutil/frame.h>
// #include <libavutil/timestamp.h>
// #include <libavformat/avformat.h>
import "C"
