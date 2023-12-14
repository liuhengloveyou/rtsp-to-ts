package rtsp2ts

import (
	"log"

	"github.com/bluenviron/gortsplib/v4"
	"github.com/bluenviron/gortsplib/v4/pkg/base"
	"github.com/bluenviron/gortsplib/v4/pkg/format"
	"github.com/bluenviron/gortsplib/v4/pkg/format/rtph264"
	"github.com/bluenviron/mediacommon/pkg/codecs/h264"
	"github.com/pion/rtp"
)

var rtspClient *gortsplib.Client
var rtpDec *rtph264.Decoder
var iframeReceived bool

func InitRtsp(url string) error {
	// parse URL
	u, err := base.ParseURL(url)
	if err != nil {
		return err
	}

	rtspClient = &gortsplib.Client{}

	// connect to the server
	err = rtspClient.Start(u.Scheme, u.Host)
	if err != nil {
		panic(err)
	}

	// find published medias
	desc, _, err := rtspClient.Describe(u)
	if err != nil {
		panic(err)
	}

	// find the H264 media and format
	var forma *format.H264
	medi := desc.FindFormat(&forma)
	if medi == nil {
		panic("media not found")
	}

	// setup RTP/H264 -> H264 decoder
	rtpDec, err = forma.CreateDecoder()
	if err != nil {
		panic(err)
	}

	// if SPS and PPS are present into the SDP, send them to the decoder
	if forma.SPS != nil {
		Decode(forma.SPS)
	}
	if forma.PPS != nil {
		Decode(forma.PPS)
	}

	// setup a single media
	_, err = rtspClient.Setup(desc.BaseURL, medi, 0, 0)
	if err != nil {
		panic(err)
	}

	// called when a RTP packet arrives
	rtspClient.OnPacketRTP(medi, forma, OnPacketRTP)

	// start playing
	_, err = rtspClient.Play(nil)
	if err != nil {
		panic(err)
	}

	return nil
}

func OnPacketRTP(pkt *rtp.Packet) {
	// extract access units from RTP packets
	au, err := rtpDec.Decode(pkt)
	if err != nil {
		if err != rtph264.ErrNonStartingPacketAndNoPrevious && err != rtph264.ErrMorePacketsNeeded {
			log.Printf("ERR: %v", err)
		}
		return
	}

	// wait for an I-frame
	if !iframeReceived {
		if !h264.IDRPresent(au) {
			log.Printf("waiting for an I-frame")
			return
		}
		iframeReceived = true
	}

	for _, nalu := range au {
		Decode(nalu)
	}
}
