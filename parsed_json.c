#include <stdio.h>
#include <json-c/json.h>
#include "os2021_thread_api.h"
void ParsedJson()
{
    struct json_object *parsed_json; // parse all thread info to this object
    /*thread object*/
    struct json_object *thread;
    struct json_object *name;
    struct json_object *entry;
    struct json_object *priority;
    struct json_object *cancel;
    size_t n_thread; // size of Threads
    size_t i;
    int q; // entry function error handler

    parsed_json = json_object_from_file("init_threads.json");
    json_object_object_get_ex(parsed_json, "Threads", &parsed_json);

    n_thread = json_object_array_length(parsed_json);

    // create thread one by one
    for (i = 0; i < n_thread; i++)
    {
        thread = json_object_array_get_idx(parsed_json, i);
        json_object_object_get_ex(thread, "name", &name);
        json_object_object_get_ex(thread, "entry function", &entry);
        json_object_object_get_ex(thread, "priority", &priority);
        json_object_object_get_ex(thread, "cancel mode", &cancel);

        // Create thread structure and enqueue
        q = OS2021_ThreadCreate((char *)json_object_get_string(name), (char *)json_object_get_string(entry), (char *)json_object_get_string(priority), json_object_get_int(cancel));
        if (q == -1)
            printf("Incorrect entry function.\n");
    }

    return;
}
