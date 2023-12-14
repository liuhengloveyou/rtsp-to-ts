package main

import (
	"fmt"
	"image"
	"image/jpeg"
	"log"
	"os"
	"strconv"
	"time"
	"unsafe"
)

// #cgo windows CFLAGS: -DCGO_OS_WINDOWS=1
// #cgo darwin CFLAGS: -DCGO_OS_DARWIN=1
// #cgo linux CFLAGS: -DCGO_OS_LINUX=1
// #cgo CFLAGS: -I. -I D:/dev/av/ffmpeg-6.1-full_build-shared/include
// #cgo LDFLAGS: -L D:/dev/av/ffmpeg-6.1-full_build-shared/lib -lavcodec -lavformat -lswscale -lswresample -lavutil -lavfilter
//
// #include <libavcodec/avcodec.h>
// #include <libavutil/imgutils.h>
// #include <libswscale/swscale.h>
// #include <libavutil/frame.h>
// #include <libavutil/timestamp.h>
// #include <libavformat/avformat.h>
//
// #include "c_api.h"
import "C"

func InitDecoder() error {
	rst := C.init_decoder()
	fmt.Println("C.init_decoder: ", rst)

	return nil
}

func CloseDecoder() {
	C.close_decoder()
}

func Decode(nalu []byte) (image.Image, error) {
	nalu = append([]uint8{0x00, 0x00, 0x00, 0x01}, []uint8(nalu)...)

	// send NALU to decoder
	var avPacket C.AVPacket
	avPacket.data = (*C.uint8_t)(C.CBytes(nalu))
	// defer C.free(unsafe.Pointer(avPacket.data))
	avPacket.size = C.int(len(nalu))

	var dstFrame *C.AVFrame
	res := C.decode(&avPacket, &dstFrame)
	if res < 0 {
		return nil, nil
	}

	// dstFrameSize := C.av_image_get_buffer_size((int32)(dstFrame.format), dstFrame.width, dstFrame.height, 1)
	// dstFramePtr := (*[1 << 30]uint8)(unsafe.Pointer(dstFrame.data[0]))[:dstFrameSize:dstFrameSize]

	// // embed frame into an image.Image
	// img := &image.RGBA{
	// 	Pix:    dstFramePtr,
	// 	Stride: 4 * (int)(dstFrame.width),
	// 	Rect: image.Rectangle{
	// 		Max: image.Point{(int)(dstFrame.width), (int)(dstFrame.height)},
	// 	},
	// }
	// fmt.Println("saveToFile......")
	// // convert frame to JPEG and save to file
	// err := saveToFile(img)
	// if err != nil {
	// 	panic(err)
	// }

	return nil, nil
}

func InitMux(fn string) {
	C.init_mux(C.CString(fn))
}

func frameData(frame *C.AVFrame) **C.uint8_t {
	return (**C.uint8_t)(unsafe.Pointer(&frame.data[0]))
}

func frameLineSize(frame *C.AVFrame) *C.int {
	return (*C.int)(unsafe.Pointer(&frame.linesize[0]))
}

// h264Decoder is a wrapper around FFmpeg's H264 decoder.
type h264Decoder struct {
	codecCtx    *C.AVCodecContext
	srcFrame    *C.AVFrame
	swsCtx      *C.struct_SwsContext
	dstFrame    *C.AVFrame
	dstFramePtr []uint8
}

// newH264Decoder allocates a new h264Decoder.
func newH264Decoder() (*h264Decoder, error) {
	codec := C.avcodec_find_decoder(C.AV_CODEC_ID_H264)
	if codec == nil {
		return nil, fmt.Errorf("avcodec_find_decoder() failed")
	}

	codecCtx := C.avcodec_alloc_context3(codec)
	if codecCtx == nil {
		return nil, fmt.Errorf("avcodec_alloc_context3() failed")
	}

	res := C.avcodec_open2(codecCtx, codec, nil)
	if res < 0 {
		C.avcodec_close(codecCtx)
		return nil, fmt.Errorf("avcodec_open2() failed")
	}

	srcFrame := C.av_frame_alloc()
	if srcFrame == nil {
		C.avcodec_close(codecCtx)
		return nil, fmt.Errorf("av_frame_alloc() failed")
	}

	return &h264Decoder{
		codecCtx: codecCtx,
		srcFrame: srcFrame,
	}, nil
}

// close closes the decoder.
func (d *h264Decoder) close() {
	if d.dstFrame != nil {
		C.av_frame_free(&d.dstFrame)
	}

	if d.swsCtx != nil {
		C.sws_freeContext(d.swsCtx)
	}

	C.av_frame_free(&d.srcFrame)
	C.avcodec_close(d.codecCtx)
}

func (d *h264Decoder) decode(nalu []byte) (image.Image, error) {
	nalu = append([]uint8{0x00, 0x00, 0x00, 0x01}, []uint8(nalu)...)

	// send NALU to decoder
	var avPacket C.AVPacket
	avPacket.data = (*C.uint8_t)(C.CBytes(nalu))
	defer C.free(unsafe.Pointer(avPacket.data))
	avPacket.size = C.int(len(nalu))
	res := C.avcodec_send_packet(d.codecCtx, &avPacket)
	if res < 0 {
		return nil, nil
	}

	// receive frame if available
	res = C.avcodec_receive_frame(d.codecCtx, d.srcFrame)
	if res < 0 {
		return nil, nil
	}

	// if frame size has changed, allocate needed objects
	if d.dstFrame == nil || d.dstFrame.width != d.srcFrame.width || d.dstFrame.height != d.srcFrame.height {
		if d.dstFrame != nil {
			C.av_frame_free(&d.dstFrame)
		}

		if d.swsCtx != nil {
			C.sws_freeContext(d.swsCtx)
		}

		d.dstFrame = C.av_frame_alloc()
		d.dstFrame.format = C.AV_PIX_FMT_RGBA
		d.dstFrame.width = d.srcFrame.width
		d.dstFrame.height = d.srcFrame.height
		d.dstFrame.color_range = C.AVCOL_RANGE_JPEG
		res = C.av_frame_get_buffer(d.dstFrame, 1)
		if res < 0 {
			return nil, fmt.Errorf("av_frame_get_buffer() failed")
		}

		d.swsCtx = C.sws_getContext(d.srcFrame.width, d.srcFrame.height, C.AV_PIX_FMT_YUV420P,
			d.dstFrame.width, d.dstFrame.height, (int32)(d.dstFrame.format), C.SWS_BILINEAR, nil, nil, nil)
		if d.swsCtx == nil {
			return nil, fmt.Errorf("sws_getContext() failed")
		}

		dstFrameSize := C.av_image_get_buffer_size((int32)(d.dstFrame.format), d.dstFrame.width, d.dstFrame.height, 1)
		d.dstFramePtr = (*[1 << 30]uint8)(unsafe.Pointer(d.dstFrame.data[0]))[:dstFrameSize:dstFrameSize]
	}

	// convert color space from YUV420 to RGBA
	res = C.sws_scale(d.swsCtx, frameData(d.srcFrame), frameLineSize(d.srcFrame),
		0, d.srcFrame.height, frameData(d.dstFrame), frameLineSize(d.dstFrame))
	if res < 0 {
		return nil, fmt.Errorf("sws_scale() failed")
	}

	// embed frame into an image.Image
	return &image.RGBA{
		Pix:    d.dstFramePtr,
		Stride: 4 * (int)(d.dstFrame.width),
		Rect: image.Rectangle{
			Max: image.Point{(int)(d.dstFrame.width), (int)(d.dstFrame.height)},
		},
	}, nil
}

func saveToFile(img image.Image) error {
	// create file
	fname := strconv.FormatInt(time.Now().UnixNano()/int64(time.Millisecond), 10) + ".jpg"
	f, err := os.Create(fname)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	log.Println("saving", fname)

	// convert to jpeg
	return jpeg.Encode(f, img, &jpeg.Options{
		Quality: 60,
	})
}
