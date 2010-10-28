#include <u.h>
#include <stdio.h>
#include <Carbon/Carbon.h>

AUTOFRAMEWORK(Carbon)

static OSErr Handler(const AppleEvent *event, AppleEvent *reply, long handlerRefcon);

int
main(void)
{
	AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, Handler, 0, false);
	RunApplicationEventLoop();
	return 0;
}

static OSErr
GetFullPathname(FSSpec *fss, char *path, int len)
{
        FSRef fsr;
        OSErr err;

        *path = '\0';
        err = FSpMakeFSRef(fss, &fsr);
        if (err == fnfErr) {
                /* FSSpecs can point to non-existing files, fsrefs can't. */
                FSSpec fss2;
                int tocopy;

                err = FSMakeFSSpec(fss->vRefNum, fss->parID,
                                   (unsigned char*)"", &fss2);
                if (err)
                        return err;
                err = FSpMakeFSRef(&fss2, &fsr);
                if (err)
                        return err;
                err = (OSErr)FSRefMakePath(&fsr, (unsigned char*)path, len-1);
                if (err)
                        return err;
                /* This part is not 100% safe: we append the filename part, but
                ** I'm not sure that we don't run afoul of the various 8bit
                ** encodings here. Will have to look this up at some point...
                */
                strcat(path, "/");
                tocopy = fss->name[0];
                if ((strlen(path) + tocopy) >= len)
                        tocopy = len - strlen(path) - 1;
                if (tocopy > 0)
                        strncat(path, (char*)fss->name+1, tocopy);
        }
        else {
                if (err)
                        return err;
                err = (OSErr)FSRefMakePath(&fsr, (unsigned char*)path, len);
                if (err)
                        return err;
        }
        return 0;
}

static void
chk(int err)
{
	if(err != 0) {
		printf("err %d\n", err);
		exit(1);
	}
}

static OSErr
Handler(const AppleEvent *event, AppleEvent *reply, long handlerRefcon)
{
	AEDesc list;
	DescType type;
	FSSpec f;
	AEKeyword keyword;
	Size actual;
	long len;
	char s[1000];

	chk(AEGetParamDesc(event, keyDirectObject, typeAEList, &list));
	chk(AECountItems(&list, &len));
	chk(AEGetNthPtr(&list, 1, typeFSS, &keyword, &type, (Ptr*)&f, sizeof(FSSpec), &actual));
	chk(GetFullPathname(&f, s, sizeof s));
	printf("%s\n", s);
	fflush(stdout);

	// uncomment to keep handling more open events
	exit(0);
}
