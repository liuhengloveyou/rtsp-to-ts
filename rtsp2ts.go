package rtsp2ts

import (
	"fmt"
	"time"

	"github.com/bluenviron/gortsplib/v4"
	"github.com/bluenviron/gortsplib/v4/pkg/base"
	"github.com/bluenviron/gortsplib/v4/pkg/description"
	"github.com/bluenviron/gortsplib/v4/pkg/format"
	"github.com/liuhengloveyou/rtsp-to-ts/asyncwriter"
	"github.com/liuhengloveyou/rtsp-to-ts/stream"
	"github.com/liuhengloveyou/rtsp-to-ts/unit"
	"github.com/pion/rtp"
)

type RtspToTS struct {
	UdpMaxPayloadSize int

	rtspClient *gortsplib.Client
	stream     *stream.Stream
	writer     *asyncwriter.Writer

	iframeReceived bool

	url    *base.URL
	urlStr string
}

func NewRtspToTS(url string) (*RtspToTS, error) {
	// parse URL
	u, err := base.ParseURL(url)
	if err != nil {
		return nil, err
	}

	return &RtspToTS{
		writer:            asyncwriter.New(1024),
		UdpMaxPayloadSize: 1472,
		url:               u,
		urlStr:            url,
	}, nil
}
func (p *RtspToTS) Run() error {
	p.rtspClient = &gortsplib.Client{}

	// connect to the server
	if err := p.rtspClient.Start(p.url.Scheme, p.url.Host); err != nil {
		return err
	}

	// find published medias
	desc, _, err := p.rtspClient.Describe(p.url)
	if err != nil {
		panic(err)
	}

	err = p.rtspClient.SetupAll(desc.BaseURL, desc.Medias)
	if err != nil {
		return err
	}

	if p.stream, err = stream.New(
		p.UdpMaxPayloadSize,
		desc,
		false,
	); err != nil {
		return err
	}

	for _, medi := range desc.Medias {
		for _, forma := range medi.Formats {
			cmedi := medi
			cforma := forma

			p.stream.AddReader(p.writer, cmedi, cforma, func(u unit.Unit) error {
				fmt.Println(">>>>>>>>>>>>>>>>>>", cmedi.Type, cforma.Codec())
				// tunit := u.(*unit.AV1)

				// if tunit.TU == nil {
				// 	return nil
				// }

				// packets, err := encoder.Encode(tunit.TU)
				// if err != nil {
				// 	return nil //nolint:nilerr
				// }

				// for _, pkt := range packets {
				// 	pkt.Timestamp += tunit.RTPPackets[0].Timestamp
				// 	track.WriteRTP(pkt) //nolint:errcheck
				// }

				return nil
			})
		}
	}

	// called when a RTP packet arrives
	p.rtspClient.OnPacketRTPAny(func(medi *description.Media, forma format.Format, pkt *rtp.Packet) {

		pts, ok := p.rtspClient.PacketPTS(medi, pkt)
		if !ok {
			return
		}

		p.stream.WriteRTPPacket(medi, forma, pkt, time.Now(), pts)

		return
		// 		// find the H264 media and format
		// var forma *format.H264
		// medi := desc.FindFormat(&forma)
		// if medi == nil {
		// 	panic("media not found")
		// }

		// // setup RTP/H264 -> H264 decoder
		// p.rtpDecoder, err = forma.CreateDecoder()
		// if err != nil {
		// 	panic(err)
		// }

		// // if SPS and PPS are present into the SDP, send them to the decoder
		// if forma.SPS != nil {
		// 	Decode(forma.SPS)
		// }
		// if forma.PPS != nil {
		// 	Decode(forma.PPS)
		// }

		// extract access units from RTP packets
		// au, err := p.rtpDecoder.Decode(pkt)
		// if err != nil {
		// 	if err != rtph264.ErrNonStartingPacketAndNoPrevious && err != rtph264.ErrMorePacketsNeeded {
		// 		log.Printf("ERR: %v", err)
		// 	}
		// 	return
		// }

		// // wait for an I-frame
		// if !p.iframeReceived {
		// 	if !h264.IDRPresent(au) {
		// 		log.Printf("waiting for an I-frame")
		// 		return
		// 	}
		// 	p.iframeReceived = true
		// }

		// for _, nalu := range au {
		// 	Decode(nalu)
		// }
	})

	// start playing
	_, err = p.rtspClient.Play(nil)
	if err != nil {
		panic(err)
	}

	p.writer.Start()

	select {
	// case <-pc.Disconnected():
	// 	writer.Stop()
	// 	return 0, fmt.Errorf("peer connection closed")

	case err := <-p.writer.Error():
		panic(err)

		// case <-s.ctx.Done():
		// 	writer.Stop()
		// 	return 0, fmt.Errorf("terminated")
	}

	return nil
}
