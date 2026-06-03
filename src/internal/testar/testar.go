// Package testar is a lightweight wrapper around txtar for use in tests.
package testar

import (
	"bytes"
	"os"
	"slices"
	"testing"

	"golang.org/x/tools/txtar"
)

type Archive struct {
	Path    string
	Comment []byte
	Files   []*File
}

type File struct {
	Name string
	Data []byte
}

func Open(t *testing.T, path string) *Archive {
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	a := txtar.Parse(data)
	var files []*File
	for _, f := range a.Files {
		data := f.Data
		if bytes.HasSuffix(data, []byte("^D\n")) {
			data = data[:len(data)-len("^D\n")]
		}
		data = bytes.ReplaceAll(data, []byte("^@"), []byte{0})
		data = bytes.ReplaceAll(data, []byte("^J\n"), []byte("\n"))
		if data == nil {
			data = []byte{}
		}
		files = append(files, &File{Name: f.Name, Data: data})
	}
	return &Archive{path, a.Comment, files}
}

func Encode(data []byte) []byte {
	data = bytes.ReplaceAll(data, []byte{0}, []byte("^@")) // always copies data
	data = bytes.ReplaceAll(data, []byte(" \n"), []byte(" ^J\n"))
	data = bytes.ReplaceAll(data, []byte("\t\n"), []byte("	^J\n"))
	if len(data) > 0 && data[len(data)-1] != '\n' {
		data = append(data, "^D\n"...)
	}
	return data
}

func (a *Archive) Save(t *testing.T) {
	ar := &txtar.Archive{
		Comment: a.Comment,
	}
	for _, f := range a.Files {
		ar.Files = append(ar.Files, txtar.File{Name: f.Name, Data: Encode(f.Data)})
	}
	if err := os.WriteFile(a.Path, txtar.Format(ar), 0666); err != nil {
		t.Fatal(err)
	}
}

func (a *Archive) Find(name string) (int, *File) {
	for i, f := range a.Files {
		if f.Name == name {
			return i, f
		}
	}
	return -1, nil
}

func (a *Archive) ReadFile(name string) []byte {
	_, f := a.Find(name)
	if f == nil {
		return nil
	}
	return slices.Clone(f.Data)
}

func (a *Archive) WriteFile(name string, data []byte) {
	data = slices.Clone(data)
	_, f := a.Find(name)
	if f != nil {
		f.Data = data
	} else {
		a.Files = append(a.Files, &File{Name: name, Data: data})
	}
}

func (a *Archive) Delete(name string) {
	i, _ := a.Find(name)
	if i < 0 {
		return
	}
	a.Files = slices.Delete(a.Files, i, i+1)
}
