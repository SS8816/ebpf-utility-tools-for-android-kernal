#ifndef CAMTRACE_H
#define CAMTRACE_H

#define TASK_COMM_LEN 16
#define ENTITY_LEN    32

#define EVENT_BINDER_CALL          20
#define EVENT_APP_SESSION          21
#define EVENT_FRAME_BUFFER_QBUF    30
#define EVENT_FRAME_BUFFER_DQBUF   31
#define EVENT_FRAME_DONE           32
#define EVENT_CAPTURE_REQUEST      33
#define EVENT_DELEGATION_EDGE      34

#define BUF_STATE_UNKNOWN 0
#define BUF_STATE_QUEUED   1
#define BUF_STATE_FILLED   2
#define BUF_STATE_SHARED   3
#define BUF_STATE_ACTIVE   4
#define BUF_STATE_CLOSED   5

struct cam_event {
    __u64 timestamp;
    __u32 pid;
    __u32 tgid;
    __u32 uid;
    char  comm[TASK_COMM_LEN];
    __u32 event_type;
    __u32 camera_id;
    __u32 buffer_id;
    __u64 extra_1;
    __u64 extra_2;
    char  entity[ENTITY_LEN];
};

#endif
