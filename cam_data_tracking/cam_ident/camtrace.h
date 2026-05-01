#ifndef CAMTRACE_H
#define CAMTRACE_H

#define TASK_COMM_LEN 16

#define EVENT_CAMERA_OPEN          1
#define EVENT_CAMERA_CLOSE         2
#define EVENT_CAMERA_BINDER_REQ    3
#define EVENT_STREAM_START         4
#define EVENT_STREAM_STOP          5
#define EVENT_BUFFER_ALLOC         6
#define EVENT_BUFFER_QUEUE         7
#define EVENT_BUFFER_DEQUEUE       8
#define EVENT_BUFFER_CREATED       9
#define EVENT_BUFFER_FILLED        10
#define EVENT_BUFFER_SHARED        11

#define BUF_STATE_QUEUED  0
#define BUF_STATE_FILLED  1
#define BUF_STATE_SHARED  2

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
};

struct buffer_info {
    __u64 timestamp;
    __u32 owner_pid;
    __u32 state;
};

#endif