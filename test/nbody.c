#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <jrpc.h>
#include <math.h>

#include "debug.h"

typedef struct body_s
{
    double x;
    double y;
    double vx;
    double vy;
    double mass;
} body;

double length(double x1, double y1, double x2, double y2)
{
    return pow(pow(x1 - x2, 2) + pow(y1 - y2, 2), 0.5);
}

void update_body(body *bodies, body *tmp_bodies, int count, int index, double dt, int micro_step_count)
{
    assert(bodies && tmp_bodies);
    body update = bodies[index];
    for(int sc = 0; sc < micro_step_count; sc ++)
    {
        double f_x = 0;
        double f_y = 0;
        for(int i = 0; i < count; i ++)
        {
            if(i == index) continue;
            double d = length(bodies[i].x, bodies[i].y, update.x, update.y);
            if(d == 0) continue;
            double f = bodies[i].mass * update.mass / d;
            f_x += f * (bodies[i].x - update.x) / d;
            f_y += f * (bodies[i].y - update.y) / d;
        }
        assert(update.mass > 0);
        update.vx = update.vx + f_x / update.mass * dt;
        update.vy = update.vy + f_y / update.mass * dt;
        update.x = update.vx * dt + update.x;
        update.y = update.vy * dt + update.y;
    }

    tmp_bodies[index] = update;
}

const char *task_name = "task_nbody.so";

extern json_object *run_simulation(json_object *params)
{
    //edsm_task_send_up_call(task_name, edsm_proto_local_id(), 1, NULL);
    json_object *j_bodies = json_object_array_get_idx(params, 0);
    int body_count = json_object_array_length(j_bodies);
    body *bodies = malloc(sizeof(body) * body_count);
    body *tmp_bodies = malloc(sizeof(body) * body_count);
    for(int i = 0; i < body_count; i++)
    {
        json_object *j_body = json_object_array_get_idx(j_bodies, i);
        bodies[i].x = json_object_get_double(json_object_array_get_idx(j_body, 0));
        bodies[i].y = json_object_get_double(json_object_array_get_idx(j_body, 1));
        bodies[i].vx = 0;
        bodies[i].vy = 0;
        bodies[i].mass = json_object_get_double(json_object_array_get_idx(j_body, 2));
    }
    
    json_object *j_timestep = json_object_array_get_idx(params, 1);
    double timestep = json_object_get_double(j_timestep);

    json_object *j_step_count = json_object_array_get_idx(params, 2);
    int32_t step_count = json_object_get_int(j_step_count);

    json_object *j_micro_step_count = json_object_array_get_idx(params, 3);
    int32_t micro_step_count = json_object_get_int(j_micro_step_count);

    DEBUG_MSG("Timestep %lf for %d steps", timestep, step_count);
    for(int t = 0; t < step_count; t++)
    {
        for(int i = 0; i < body_count; i++)
        {
            update_body(bodies, tmp_bodies, body_count, i, timestep / micro_step_count, micro_step_count);
        }
        body *swap = bodies;
        bodies = tmp_bodies;
        tmp_bodies = swap;
    }

    for(int i = 0; i < body_count; i++)
    {
        json_object *j_body = json_object_array_get_idx(j_bodies, i);
        json_object_array_put_idx(j_body, 0, json_object_new_double(bodies[i].x));
        json_object_array_put_idx(j_body, 1, json_object_new_double(bodies[i].y));
        json_object_array_put_idx(j_body, 2, json_object_new_double(bodies[i].mass));
    }

    return j_bodies;
}

extern int up_call(struct edsm_task_information *task, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    if(event == 0)
    {
        struct jrpc_server *server = jrpc_server_init(8764);
        if(server == NULL) return SUCCESS;
        jrpc_server_register(server, run_simulation, "run_simulation");
        jrpc_server_run(server);
    }
    else if(event == 1)
    {

    }
    return SUCCESS;
}
