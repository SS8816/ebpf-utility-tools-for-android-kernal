#ifndef CAMTRACE_H
#define CAMTRACE_H

#define TASK_COMM_LEN 16
#define ENTITY_LEN    32

/* Event types */

#define EVENT_BINDER_CALL          20
#define EVENT_APP_SESSION          21

#define EVENT_FRAME_BUFFER_QBUF    30
#define EVENT_FRAME_BUFFER_DQBUF   31
#define EVENT_FRAME_DONE           32
#define EVENT_CAPTURE_REQUEST      33
#define EVENT_DELEGATION_EDGE      34

/* Generic event */

struct cam_event {
    unsigned long long timestamp;

    unsigned int pid;
    unsigned int tgid;
    unsigned int uid;

    char comm[TASK_COMM_LEN];

    unsigned int event_type;

    unsigned int camera_id;
    unsigned int buffer_id;

    unsigned long long extra_1;
    unsigned long long extra_2;

    char entity[ENTITY_LEN];
};

#endif