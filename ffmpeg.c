// Python module for playing audio
// part of MusicPlayer, https://github.com/albertz/music-player
// Copyright (c) 2012, Albert Zeyer, www.az2000.de
// All rights reserved.
// This code is under the 2-clause BSD license, see License.txt in the root directory of this project.

// compile:
// gcc -c ffmpeg*.{c,cpp} -I /System/Library/Frameworks/Python.framework/Headers/
// libtool -dynamic -o ffmpeg.so ffmpeg*.o -framework Python -lavformat -lavutil -lavcodec -lswresample -lportaudio -lc

// loosely based on ffplay.c
// https://github.com/FFmpeg/ffmpeg/blob/master/ffplay.c

// Similar to PyFFmpeg. http://code.google.com/p/pyffmpeg/
// This module is intended to be much simpler/high-level, though.

// Pyton interface:
//	createPlayer() -> player object with:
//		queue: song generator
//		playing: True or False, initially False
//		volume: 1.0 is norm; this is just a factor to each sample. default is 0.9
//		volumeSmoothClip: smooth clipping, see below. set to (1,1) to disable. default is (0.95,10)
//		curSong: current song (read only)
//		curSongPos: current song pos (read only)
//		curSongLen: current song len (read only)
//		curSongGainFactor: current song gain. read from song.gain (see below). can also be written
//		seekAbs(t) / seekRel(t): seeking functions (t in seconds, accepts float)
//		nextSong(): skip to next song function
//	song object expected interface:
//		readPacket(bufSize): should return some string
//		seekRaw(offset, whence): should seek and return the current pos
//		gain: some gain in decible, e.g. calculated by calcReplayGain. if not present, is ignored
//		url: some url, can be anything printable (not used at the moment)
//	and other functions, see their embedded doc ...


#include "ffmpeg.h"






// this is mostly safe to call.
// returns a newly allocated c-string.
char* objStrDup(PyObject* obj) {
	PyGILState_STATE gstate = PyGILState_Ensure();
	char* str = NULL;
	PyObject* earlierError = PyErr_Occurred();
	if(!obj)
		str = "<None>";
	else if(PyString_Check(obj))
		str = PyString_AsString(obj);
	else {
		PyObject* strObj = NULL;
		if(PyUnicode_Check(obj))
			strObj = PyUnicode_AsUTF8String(obj);
		else {
			PyObject* unicodeObj = PyObject_Unicode(obj);
			if(unicodeObj) {
				strObj = PyUnicode_AsUTF8String(unicodeObj);
				Py_DECREF(unicodeObj);
			}
		}
		if(strObj) {
			str = PyString_AsString(strObj);
			Py_DECREF(strObj);
		}
		else
			str = "<CantConvertToString>";
	}
	if(!earlierError && PyErr_Occurred())
		PyErr_Print();
	assert(str);
	str = strdup(str);
	PyGILState_Release(gstate);
	return str;
}

// returns a newly allocated c-string.
char* objAttrStrDup(PyObject* obj, const char* attrStr) {
	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject* attrObj = PyObject_GetAttrString(obj, attrStr);
	char* str = objStrDup(attrObj);
	Py_XDECREF(attrObj);
	PyGILState_Release(gstate);
	return str;
}









static PyMethodDef module_methods[] = {
	{"createPlayer",	(PyCFunction)pyCreatePlayer,	METH_NOARGS,	"creates new player"},
	{"getMetadata",		pyGetMetadata,	METH_VARARGS,	"get metadata for Song"},
	{"calcAcoustIdFingerprint",		pyCalcAcoustIdFingerprint,	METH_VARARGS,	"calculate AcoustID fingerprint for Song"},
	{"calcBitmapThumbnail",		(PyCFunction)pyCalcBitmapThumbnail,	METH_VARARGS|METH_KEYWORDS,	"calculate bitmap thumbnail for Song"},
	{"calcReplayGain",		(PyCFunction)pyCalcReplayGain,	METH_VARARGS|METH_KEYWORDS,	"calculate ReplayGain for Song"},
	{"setFfmpegLogLevel",		pySetFfmpegLogLevel,	METH_VARARGS,	"set FFmpeg log level (av_log_set_level)"},
	{NULL,				NULL}	/* sentinel */
};

PyDoc_STRVAR(module_doc,
"FFmpeg player.");

static PyObject* EventClass = NULL;

static void init() {
	PyEval_InitThreads(); /* Start the interpreter's thread-awareness */
	initPlayerOutput();
	initPlayerDecoder();
}


PyMODINIT_FUNC
initffmpeg(void)
{
	//printf("initffmpeg\n");
	init();
	if (PyType_Ready(&Player_Type) < 0)
		Py_FatalError("Can't initialize player type");
	PyObject* m = Py_InitModule3("ffmpeg", module_methods, module_doc);
	if(!m) {
		Py_FatalError("Can't initialize ffmpeg module");
		return;
	}
	
	if(EventClass == NULL) {
		PyObject* classDict = PyDict_New();
		assert(classDict);
		PyObject* className = PyString_FromString("Event");
		assert(className);
		EventClass = PyClass_New(NULL, classDict, className);
		assert(EventClass);
		Py_XDECREF(classDict); classDict = NULL;
		Py_XDECREF(className); className = NULL;
	}
	
	if(EventClass) {
		Py_INCREF(EventClass);
		PyModule_AddObject(m, "Event", EventClass); // takes the ref
	}
}
