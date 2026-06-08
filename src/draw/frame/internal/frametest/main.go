package main

import (
	"log"

	"9fans.net/go/draw"
	"9fans.net/go/draw/frame"
)

var display *draw.Display
var cols [frame.NCOL]*draw.Image
var f frame.Frame

func main() {
	d, err := draw.Init(nil, "", "frametest", "")
	if err != nil {
		log.Fatal(err)
	}
	display = d

	cols = [...]*draw.Image{
		frame.BACK:  d.AllocImageMix(draw.PaleBlueGreen, draw.White),
		frame.HIGH:  d.AllocImageMix(draw.PaleGreyGreen, draw.PaleGreyGreen),
		frame.BORD:  d.AllocImageMix(draw.PurpleBlue, draw.PurpleBlue),
		frame.TEXT:  d.Black,
		frame.HTEXT: d.Black,
	}
	mousectl := d.InitMouse()
	kbdctl := d.InitKeyboard()
	redraw(true)

Loop:
	for {
		d.Flush()
		select {
		case <-mousectl.Resize:
			redraw(true)
		case <-mousectl.C:
		case r := <-kbdctl.C:
			if r == 'q' {
				break Loop
			}
		}
	}
}

func redraw(new bool) {
	d := display
	if new {
		if err := d.Attach(draw.RefMesg); err != nil {
			log.Fatalf("can't reattach to window: %v", err)
		}
	}
	d.Image.Draw(d.Image.R, cols[frame.BACK], display.Opaque, draw.ZP)
	d.Image.Border(d.Image.R, 4, cols[frame.BORD], draw.ZP)
	f.Clear(false)
	f.Init(d.Image.R.Inset(4), d.Font, d.Image, cols[:])
	f.Insert([]rune("hello, world!\n\tthis is the time!\nhi"), 0)
	f.Insert([]rune("curl..."), 7)
	f.Insert([]rune("EOF"), f.NumChars)
}
