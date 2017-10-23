#include <Python.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>

#include <jack/jack.h>


/* JACK callbacks */

typedef struct _thread_info {
    jack_client_t* client;
    unsigned int channels;
    int bitdepth;
    volatile int can_capture;
    volatile int can_process;
    volatile int status;
} jack_thread_info_t;

jack_client_t* client;

jack_port_t* output_port1;
jack_port_t* output_port2;
jack_port_t* input_port1;
jack_port_t* input_port2;


static void signal_handler(int sig) {
  jack_client_close(client);
  fprintf(stderr, "signal received, exiting ...\n");
  exit(0);
}


void jack_shutdown(void* arg) {
  exit(1);
}


int process(jack_nframes_t nframes, void *arg) {
  jack_thread_info_t* info = (jack_thread_info_t*)arg;

  /* Do nothing until we're ready to begin. */
  if ((!info->can_process) || (!info->can_capture)) {
    return 0;
  }

  jack_default_audio_sample_t* out1 = jack_port_get_buffer(output_port1, nframes);
  jack_default_audio_sample_t* out2 = jack_port_get_buffer(output_port2, nframes);

  jack_default_audio_sample_t* in1 = jack_port_get_buffer(input_port1, nframes);
  jack_default_audio_sample_t* in2 = jack_port_get_buffer(input_port2, nframes);

  memcpy(out1, in1, nframes*sizeof(jack_default_audio_sample_t));
  memcpy(out2, in2, nframes*sizeof(jack_default_audio_sample_t));

  return 0;
}


/* JACK client setup */

void setup_jack_client() {
  jack_thread_info_t thread_info;
  memset(&thread_info, 0, sizeof(thread_info));

  if ((client = jack_client_open("jackrec", JackNullOption, NULL)) == 0) {
    fprintf(stderr, "JACK server not running?\n");
    exit(1);
  }

  thread_info.client = client;

  fprintf(stderr, "Setting up JACK callbacks\n");
  jack_set_process_callback(thread_info.client, process, &thread_info);
  jack_on_shutdown(thread_info.client, jack_shutdown, &thread_info);

  fprintf(stderr, "Activating JACK client\n");
  if (jack_activate(thread_info.client)) {
    fprintf(stderr, "Cannot activate client");
  }

  fprintf(stderr, "Registering output ports\n");
  /* create two out ports */
  output_port1 = jack_port_register(thread_info.client, "output1",
      JACK_DEFAULT_AUDIO_TYPE,
      JackPortIsOutput, 0);

  output_port2 = jack_port_register(thread_info.client, "output2",
      JACK_DEFAULT_AUDIO_TYPE,
      JackPortIsOutput, 0);

  if ((output_port1 == NULL) || (output_port2 == NULL)) {
    fprintf(stderr, "No more JACK ports available\n");
    exit(1);
  }

  fprintf(stderr, "Registering input ports\n");
  /* create two in ports */
  input_port1 = jack_port_register(thread_info.client, "input1",
      JACK_DEFAULT_AUDIO_TYPE,
      JackPortIsInput, 0);

  input_port2 = jack_port_register(thread_info.client, "input2",
      JACK_DEFAULT_AUDIO_TYPE,
      JackPortIsInput, 0);

  if ((input_port1 == NULL) || (input_port2 == NULL)) {
    fprintf(stderr, "No more JACK ports available\n");
    exit(1);
  }

   /* install a signal handler to properly quits jack client */
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  thread_info.can_process = 1;
  thread_info.can_capture = 1;
}


/* Python module magic */


/* Method definitions */

static PyObject* tapy_setup(PyObject* self, PyObject* args) {
  setup_jack_client();

  return Py_None;
}


static PyObject* tapy_shutdown(PyObject* self, PyObject* args) {
  jack_client_close(client);

  return Py_None;
}


/* Module method table */

static PyMethodDef TapyMethods[] = {
  {"setup",  tapy_setup, METH_VARARGS, "."},
  {"shutdown",  tapy_shutdown, METH_VARARGS, "."},
  {NULL, NULL, 0, NULL} /* Sentinel */
};


/* Module definition and initialisation */

#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef tapymodule = {
  PyModuleDef_HEAD_INIT,
  "tapy", /* module name */
  NULL,
  /**
   * size of per-interpreter state of the module, or -1 if the module keeps
   * state in global variables.
   */
  -1,
  TapyMethods
};

PyMODINIT_FUNC PyInit_tapy(void) {
  return PyModule_Create(&tapymodule);
}

#else // PY_MAJOR_VERSION < 3

PyMODINIT_FUNC inittapy(void) {
  (void) Py_InitModule("tapy", TapyMethods);
}

#endif
