package codec

import (
	"fmt"

	"github.com/bluenviron/mediacommon/pkg/codecs/mpeg4audio"
	"github.com/liuhengloveyou/rtsp-to-ts/unit"
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
// #include "decoder.h"
import "C"

type Decode interface {
	DecodeUnit(unit.Unit) error
}

type Decoder struct {
	Codec string

	de *C.Decoder
}

// New allocates a Processor.
func NewDecoder(codecName string) (*Decoder, error) {
	var codecID C.enum_AVCodecID = 0

	switch codecName {
	case "AV1":
		codecID = C.AV_CODEC_ID_AV1
	case "VP9":
		codecID = C.AV_CODEC_ID_VP9
	case "VP8":
		codecID = C.AV_CODEC_ID_VP8
	case "H265":
		codecID = C.AV_CODEC_ID_HEVC
	case "H264":
		codecID = C.AV_CODEC_ID_H264
	case "Opus":
		codecID = C.AV_CODEC_ID_OPUS
	case "MPEG4Audio":
		codecID = C.AV_CODEC_ID_AAC
	default:
		return nil, nil
	}

	de := C.init_decoder(codecID, nil)
	if de == nil {
		return nil, fmt.Errorf("init_decoder Err")
	}

	return &Decoder{
		Codec: codecName,
		de:    de,
	}, nil
}

func (p *Decoder) Close() {
	C.close_decoder(p.de)
}

// Decode(nalu []byte) (image.Image, error) {
func (p *Decoder) DecodeUnit(u unit.Unit) error {
	switch p.Codec {
	case "AV1":
		tunit := u.(*unit.AV1)
		if tunit.TU == nil {
			return nil
		}
	case "VP9":

	case "VP8":

	case "H265":

	case "H264":
		// tunit := u.(*unit.H264)
		// if tunit.AU == nil {
		// 	return nil
		// }
		// return p.decodeH264Unit(tunit)
	case "Opus":

	case "MPEG4Audio":
		tunit := u.(*unit.MPEG4Audio)
		if tunit.AUs == nil {
			return nil
		}
		return p.decodeMPEG4AudioUnit(tunit)
	default:
		return nil
	}

	return nil
}

func (p *Decoder) decodeH264Unit(u *unit.H264) error {
	for _, nalu := range u.AU {
		nalu = append([]uint8{0x00, 0x00, 0x00, 0x01}, []uint8(nalu)...)

		// send NALU to decoder
		var avPacket C.AVPacket
		avPacket.data = (*C.uint8_t)(C.CBytes(nalu))
		//defer C.free(unsafe.Pointer(avPacket.data))
		avPacket.size = C.int(len(nalu))
		C.decode(p.de, &avPacket)
	}

	return nil
}

func (p *Decoder) decodeMPEG4AudioUnit(u *unit.MPEG4Audio) error {
	pkts := make(mpeg4audio.ADTSPackets, len(u.AUs))

	for i, nalu := range u.AUs {
		// nalu = append([]uint8{0x00, 0x00, 0x00, 0x01}, []uint8(nalu)...)

		pkts[i] = &mpeg4audio.ADTSPacket{
			Type:         0, // mpeg4audio.ObjectTypeAACLC,
			SampleRate:   44100,
			ChannelCount: 4,
			AU:           nalu,
		}
	}

	data, _ := pkts.Marshal()
	// send NALU to decoder
	var avPacket C.AVPacket
	avPacket.data = (*C.uint8_t)(C.CBytes(data))
	//defer C.free(unsafe.Pointer(avPacket.data))
	avPacket.size = C.int(len(data))
	if rst := C.decode(p.de, &avPacket); rst != 0 {
		return fmt.Errorf("decode Err")
	}

	return nil
}
