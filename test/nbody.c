#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <jrpc.h>
#include <math.h>

#include "debug.h"
#include "memory/memory.h"
#include "utlist.h"

struct simulation
{
    uint32_t body_count;
    uint32_t bodies_id;
    uint32_t client_barrier_id;
    uint32_t completion_barrier_id;
    double timestep;
    uint32_t step_count;
    uint32_t micro_step_count;
};
struct simulation current_sim;

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

double radius(double mass)
{
    return pow(mass/200, .5);
}

void update_body(body *bodies, body *tmp_bodies, int count, int offset, int index, double dt, int micro_step_count)
{
    assert(bodies && tmp_bodies);
    body update = bodies[index + offset];
    if(update.mass <= 0) return;
    for(int sc = 0; sc < micro_step_count; sc ++)
    {
        double f_x = 0;
        double f_y = 0;
        for(int i = 0; i < count; i ++)
        {
            if(i == (index + offset) || bodies[i].mass <= 0) continue;
            double d = length(bodies[i].x, bodies[i].y, update.x, update.y);
            if(d < (radius(update.mass) + radius(bodies[i].mass)))
            {
                if(update.mass > bodies[i].mass || (update.mass == bodies[i].mass && (index + offset) > i))
                {
                    update.vx = (update.mass * update.vx + bodies[i].mass * bodies[i].vx) / (update.mass + bodies[i].mass);
                    update.vy = (update.mass * update.vy + bodies[i].mass * bodies[i].vy) / (update.mass + bodies[i].mass);
                    update.x = (update.mass * update.x + bodies[i].mass * bodies[i].x) / (update.mass + bodies[i].mass);
                    update.y = (update.mass * update.y + bodies[i].mass * bodies[i].y) / (update.mass + bodies[i].mass);
                    update.mass += bodies[i].mass;
                }
                else
                {
                    tmp_bodies[index].mass = 0;
                    return;
                }
            }
            else
            {
                double f = 6.7e-11 * bodies[i].mass * update.mass / d;
                f_x += f * (bodies[i].x - update.x) / d;
                f_y += f * (bodies[i].y - update.y) / d;
            }
        }
        update.vx = update.vx + f_x / update.mass * dt;
        update.vy = update.vy + f_y / update.mass * dt;
        update.x = update.vx * dt + update.x;
        update.y = update.vy * dt + update.y;
    }
    tmp_bodies[index] = update;
}

const char *task_name = "task_nbody.so";

extern json_object *init_simulation(json_object *params)
{
    //edsm_task_send_up_call(task_name, edsm_proto_local_id(), 1, NULL);
    json_object *j_bodies = json_object_array_get_idx(params, 0);
    current_sim.body_count = json_object_array_length(j_bodies);

    json_object *tmp = json_object_array_get_idx(params, 1);
    current_sim.timestep = json_object_get_double(tmp);

    tmp = json_object_array_get_idx(params, 2);
    current_sim.step_count = json_object_get_int(tmp);

    tmp = json_object_array_get_idx(params, 3);
    current_sim.micro_step_count = json_object_get_int(tmp);

    current_sim.bodies_id = edsm_dobj_create();
    current_sim.client_barrier_id = edsm_dobj_create();
    current_sim.completion_barrier_id = edsm_dobj_create();

    edsm_memory_region *bodies_region = edsm_memory_region_get(sizeof(body) * current_sim.body_count, current_sim.bodies_id);
    edsm_barrier *completion_barrier = edsm_barrier_get(current_sim.completion_barrier_id);
    edsm_barrier_arm(completion_barrier, edsm_proto_get_peer_ids());

    struct edsm_proto_peer_id *peers = edsm_proto_get_peer_ids();
    struct edsm_proto_peer_id *peer;
    LL_FOREACH(peers, peer)
    {

        edsm_message *msg = edsm_message_create(0, 20);
        edsm_message_write(msg, &current_sim, sizeof(current_sim));
        edsm_task_send_up_call(task_name, peer->id, 2, msg);
    }
    DEBUG_MSG("Waiting for region join");
    edsm_barrier_wait(completion_barrier);
    DEBUG_MSG("Region join done");

    body *bodies = bodies_region->head;

    for(int i = 0; i < current_sim.body_count; i++)
    {
        json_object *j_body = json_object_array_get_idx(j_bodies, i);
        bodies[i].x = json_object_get_double(json_object_array_get_idx(j_body, 0));
        bodies[i].y = json_object_get_double(json_object_array_get_idx(j_body, 1));
        bodies[i].vx = json_object_get_double(json_object_array_get_idx(j_body, 2));
        bodies[i].vy = json_object_get_double(json_object_array_get_idx(j_body, 3));
        bodies[i].mass = json_object_get_double(json_object_array_get_idx(j_body, 4));
    }

    edsm_barrier_notify(completion_barrier);

    json_object *output = json_object_new_array();

    json_object_array_add(output, json_object_new_int(current_sim.bodies_id));

    return output;
}

extern json_object *run_simulation(json_object *params)
{

    edsm_memory_region *bodies_region = edsm_memory_region_get(sizeof(body) * current_sim.body_count, current_sim.bodies_id);
    edsm_barrier *client_barrier = edsm_barrier_get(current_sim.client_barrier_id);
    edsm_barrier *completion_barrier = edsm_barrier_get(current_sim.completion_barrier_id);

    struct edsm_proto_peer_id *peers = edsm_proto_get_peer_ids();
    int thread_count;
    struct edsm_proto_peer_id *peer;
    LL_COUNT(peers, peer, thread_count);

    edsm_barrier_arm(completion_barrier, peers);
    peers = edsm_proto_get_peer_ids();
    edsm_barrier_arm(client_barrier, peers);

    peer = peers;
    for(int i = 0; i < thread_count; i++)
    {
        uint32_t start = i * current_sim.body_count / thread_count;
        uint32_t count = current_sim.body_count / thread_count;
        edsm_message *msg = edsm_message_create(0, 8);
        edsm_message_write(msg, &current_sim, sizeof(current_sim));
        edsm_message_write(msg, &start, sizeof(start));
        edsm_message_write(msg, &count, sizeof(count));

        edsm_task_send_up_call(task_name, peer->id, 1, msg);

        edsm_message_destroy(msg);
        peer = peer->next;
    }

    edsm_barrier_wait(client_barrier);
    edsm_barrier_notify(client_barrier);

    edsm_barrier_wait(completion_barrier);

    json_object *j_bodies = json_object_new_array();
    for(int i = 0; i < current_sim.body_count; i++)
    {
        body b = ((body *)bodies_region->head)[i];
        if(b.mass == 0) continue;
        json_object *j_body = json_object_new_array();
        json_object_array_add(j_bodies, j_body);
        json_object_array_put_idx(j_body, 0, json_object_new_double(b.x));
        json_object_array_put_idx(j_body, 1, json_object_new_double(b.y));
        json_object_array_put_idx(j_body, 2, json_object_new_double(b.mass));
    }

    return j_bodies;
}

extern int up_call(struct edsm_task_information *task, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    if(event == 0)
    {
        struct jrpc_server *server = jrpc_server_init(8764);
        if(server == NULL) return SUCCESS;
        jrpc_server_register(server, init_simulation, "init_simulation");
        jrpc_server_register(server, run_simulation, "run_simulation");
        jrpc_server_run(server);
    }
    else if(event == 1)
    {
        edsm_message_read(params, &current_sim, sizeof(current_sim));
        uint32_t start;
        edsm_message_read(params, &start, sizeof(start));
        uint32_t count;
        edsm_message_read(params, &count, sizeof(count));

        edsm_memory_region *bodies_region = edsm_memory_region_get(sizeof(body) * current_sim.body_count, current_sim.bodies_id);
        edsm_barrier *client_barrier = edsm_barrier_get(current_sim.client_barrier_id);
        edsm_barrier *completion_barrier = edsm_barrier_get(current_sim.completion_barrier_id);
        body *tmp_bodies = malloc(sizeof(body) * count);
        memset(tmp_bodies, 0, sizeof(body) * count);

        edsm_barrier_arm(client_barrier, edsm_proto_peer_id_create(peer_id));
        edsm_barrier_arm(completion_barrier, edsm_proto_peer_id_create(peer_id));

        DEBUG_MSG("Timestep %lf for %d steps %d %d", current_sim.timestep, current_sim.step_count, start, count);
        for(int t = 0; t < current_sim.step_count; t++)
        {
            for(int i = 0; i < count; i++)
            {
                update_body(bodies_region->head,
                    tmp_bodies,
                    current_sim.body_count,
                    start,
                    i,
                    current_sim.timestep / current_sim.micro_step_count,
                    current_sim.micro_step_count);
            }
        }
        edsm_barrier_notify(client_barrier);
        edsm_barrier_wait(client_barrier);
        memcpy((body *)bodies_region->head + start, tmp_bodies, sizeof(body) * count);
        edsm_barrier_notify(completion_barrier);
    }
    else if(event == 2)
    {
        edsm_message_read(params, &current_sim, sizeof(current_sim));
        edsm_memory_region_get(sizeof(body) * current_sim.body_count, current_sim.bodies_id);
        edsm_barrier *completion_barrier = edsm_barrier_get(current_sim.completion_barrier_id);
        edsm_barrier_notify(completion_barrier);
    }
    return SUCCESS;
}
