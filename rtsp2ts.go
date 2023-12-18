package rtsp2ts

import (
	"fmt"
	"strings"
	"time"

	"github.com/bluenviron/gortsplib/v4"
	"github.com/bluenviron/gortsplib/v4/pkg/base"
	"github.com/bluenviron/gortsplib/v4/pkg/description"
	"github.com/bluenviron/gortsplib/v4/pkg/format"
	"github.com/liuhengloveyou/rtsp-to-ts/asyncwriter"
	"github.com/liuhengloveyou/rtsp-to-ts/codec"
	"github.com/liuhengloveyou/rtsp-to-ts/stream"
	"github.com/liuhengloveyou/rtsp-to-ts/unit"
	"github.com/pion/rtp"
)

type RtspToTS struct {
	UdpMaxPayloadSize int

	rtspClient *gortsplib.Client
	stream     *stream.Stream
	writer     *asyncwriter.Writer

	decoder map[string]*codec.Decoder
	encoder *codec.Encoder

	iframeReceived bool

	url    *base.URL
	urlStr string
}

func NewRtspToTS(url string, fn string) (*RtspToTS, error) {
	// parse URL
	u, err := base.ParseURL(url)
	if err != nil {
		return nil, err
	}

	en := codec.NewEncoder(fn)
	if en == nil {
		return nil, nil
	}

	return &RtspToTS{
		encoder:           en,
		decoder:           make(map[string]*codec.Decoder),
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
		fmt.Printf("medi: %#v\n", medi)
		for _, forma := range medi.Formats {
			cmedi := medi
			cforma := forma

			fmt.Printf("medi: %#v\n", cforma)
			codecName := strings.ReplaceAll(cforma.Codec(), " ", "")
			codecName = strings.ReplaceAll(codecName, "-", "")
			fmt.Println("NewDecoder>>>", cmedi.Type, codecName)

			p.decoder[fmt.Sprintf("%v-%v", cmedi.Type, cforma.Codec())], err = codec.NewDecoder(codecName)
			if err != nil {
				panic(err)
			}

			p.stream.AddReader(p.writer, cmedi, cforma, func(u unit.Unit) error {
				de := p.decoder[fmt.Sprintf("%v-%v", cmedi.Type, cforma.Codec())]
				if de == nil {
					fmt.Println("decoder nil: ", cmedi.Type, cforma.Codec())
					return nil
				}

				if err := de.DecodeUnit(u); err != nil {
					// panic(err)
				}

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
