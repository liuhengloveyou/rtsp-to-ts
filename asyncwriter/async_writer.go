// Package asyncwriter contains an asynchronous writer.
package asyncwriter

import (
	"fmt"

	"github.com/bluenviron/gortsplib/v4/pkg/ringbuffer"
)

// Writer is an asynchronous writer.
type Writer struct {
	buffer *ringbuffer.RingBuffer

	// out
	err chan error
}

// New allocates a Writer.
func New(queueSize int) *Writer {
	buffer, err := ringbuffer.New(uint64(queueSize))
	if err != nil {
		panic(err)
	}

	return &Writer{
		buffer: buffer,
		err:    make(chan error),
	}
}

// Start starts the writer routine.
func (w *Writer) Start() {
	go w.run()
}

// Stop stops the writer routine.
func (w *Writer) Stop() {
	w.buffer.Close()
	<-w.err
}

// Error returns whenever there's an error.
func (w *Writer) Error() chan error {
	return w.err
}

func (w *Writer) run() {
	w.err <- w.runInner()
}

func (w *Writer) runInner() error {
	for {
		cb, ok := w.buffer.Pull()
		if !ok {
			return fmt.Errorf("terminated")
		}

		err := cb.(func() error)()
		if err != nil {
			return err
		}
	}
}

// Push appends an element to the queue.
func (w *Writer) Push(cb func() error) {
	ok := w.buffer.Push(cb)
	if !ok {
		fmt.Println("write queue is full")
	}
}
