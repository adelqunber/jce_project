#define _POSIX_C_SOURCE 200809L

#include "GUI.h"
#include "raylib.h"
#include "IPC.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700
#define MAX_NODES 15
#define NODE_RADIUS 25.0f
#define ARROW_SIZE 12.0f

#define JUMP_TIME 0.30f
#define NODE_WAIT_TIME 1.00f
#define EDGE_UNIT_TIME 0.30f

#define MAX_TRAVELERS 8

typedef struct {
    int current_path_index;
    int current_step;
    float jump_timer;
    float wait_timer;
    int waiting_at_node;
    int finished;
    int signal_sent;
} PathAnimState;

typedef struct {
    int has_position;
    int current_node;
    int next_node;

    int moving;
    float move_timer;
    float move_duration;

    int waiting_at_node;
    float wait_timer;

    int waiting_for_msg;
    int finished;
} IpcAnimState;

static const Color TRAVELER_COLORS[MAX_TRAVELERS] = {
    RED, BLUE, GREEN, PURPLE, ORANGE, PINK, LIME, GOLD
};

static const Color HEART_FILL_COLORS[MAX_TRAVELERS] = {
    { 200, 50, 50, 255 },
    { 50, 80, 200, 255 },
    { 50, 160, 50, 255 },
    { 140, 50, 180, 255 },
    { 220, 130, 20, 255 },
    { 220, 90, 160, 255 },
    { 140, 200, 20, 255 },
    { 200, 170, 0, 255 }
};

static Vector2 get_node_position(int node, int total_nodes)
{
    float cx = SCREEN_WIDTH / 2.0f;
    float cy = SCREEN_HEIGHT / 2.0f;
    float r = 240.0f;
    float angle = (2.0f * PI * (float)node) / (float)total_nodes - PI / 2.0f;

    Vector2 result;
    result.x = cx + r * cosf(angle);
    result.y = cy + r * sinf(angle);
    return result;
}

static Vector2 vector_lerp(Vector2 a, Vector2 b, float t)
{
    Vector2 result;
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    return result;
}

static int get_edge_weight(const Graph* graph, int src, int dst)
{
    if (!graph || src < 0 || src >= graph->num_nodes) return 1;

    for (Edge* e = graph->adj[src]; e != NULL; e = e->next) {
        if (e->dst == dst) return (e->weight > 0) ? e->weight : 1;
    }

    return 1;
}

static void draw_heart(Vector2 center, float size, Color fill, Color outline)
{
    int limit = (int)(size * 1.4f);

    for (int y = -limit; y <= limit; y++) {
        for (int x = -limit; x <= limit; x++) {
            float nx = (float)x / size;
            float ny = -(float)y / size;
            float v = powf(nx * nx + ny * ny - 1.0f, 3.0f) - nx * nx * ny * ny * ny;
            if (v <= 0.0f) {
                DrawPixel((int)(center.x + x), (int)(center.y + y), fill);
            }
        }
    }

    const int SEG = 80;
    Vector2 pts[SEG + 1];
    float scale = size / 21.0f;

    for (int i = 0; i <= SEG; i++) {
        float t = (2.0f * PI * (float)i) / (float)SEG;
        pts[i].x = center.x + 16.0f * powf(sinf(t), 3.0f) * scale;
        pts[i].y = center.y -
                   (13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                    2.0f * cosf(3.0f * t) - cosf(4.0f * t)) * scale;
    }

    DrawLineStrip(pts, SEG + 1, outline);
}

static void draw_arrow(Vector2 start, Vector2 end, Color color, float thickness)
{
    Vector2 d;
    d.x = end.x - start.x;
    d.y = end.y - start.y;

    float len = sqrtf(d.x * d.x + d.y * d.y);
    if (len == 0.0f) return;

    d.x /= len;
    d.y /= len;

    Vector2 line_start;
    line_start.x = start.x + d.x * NODE_RADIUS;
    line_start.y = start.y + d.y * NODE_RADIUS;

    Vector2 line_end;
    line_end.x = end.x - d.x * NODE_RADIUS;
    line_end.y = end.y - d.y * NODE_RADIUS;

    DrawLineEx(line_start, line_end, thickness, color);

    float angle = atan2f(d.y, d.x);

    Vector2 p1;
    p1.x = line_end.x - ARROW_SIZE * cosf(angle - PI / 6.0f);
    p1.y = line_end.y - ARROW_SIZE * sinf(angle - PI / 6.0f);

    Vector2 p2;
    p2.x = line_end.x - ARROW_SIZE * cosf(angle + PI / 6.0f);
    p2.y = line_end.y - ARROW_SIZE * sinf(angle + PI / 6.0f);

    DrawTriangle(line_end, p1, p2, color);
}

static void draw_edge_weight(Vector2 start, Vector2 end, int weight)
{
    Vector2 mid;
    mid.x = (start.x + end.x) / 2.0f;
    mid.y = (start.y + end.y) / 2.0f;

    const char* text = TextFormat("%d", weight);
    int tw = MeasureText(text, 20);

    DrawRectangle((int)(mid.x - (float)tw / 2.0f - 5.0f),
                  (int)(mid.y - 12.0f),
                  tw + 10,
                  24,
                  RAYWHITE);
    DrawText(text, (int)(mid.x - (float)tw / 2.0f), (int)(mid.y - 10.0f), 20, RED);
}

static void draw_edges(const Graph* graph, Vector2 pos[])
{
    for (int src = 0; src < graph->num_nodes; src++) {
        for (Edge* e = graph->adj[src]; e != NULL; e = e->next) {
            draw_arrow(pos[src], pos[e->dst], DARKGRAY, 3.0f);
            draw_edge_weight(pos[src], pos[e->dst], e->weight);
        }
    }
}

static void draw_path_highlight(const Graph* graph,
                                const int* path,
                                int path_length,
                                Vector2 pos[],
                                Color color)
{
    if (!path || path_length < 2) return;

    for (int i = 0; i < path_length - 1; i++) {
        draw_arrow(pos[path[i]], pos[path[i + 1]], color, 5.0f);
        draw_edge_weight(pos[path[i]], pos[path[i + 1]], get_edge_weight(graph, path[i], path[i + 1]));
    }
}

static void draw_nodes(const Graph* graph, Vector2 pos[])
{
    for (int i = 0; i < graph->num_nodes; i++) {
        DrawCircleV(pos[i], NODE_RADIUS, SKYBLUE);
        DrawCircleLines((int)pos[i].x, (int)pos[i].y, NODE_RADIUS, DARKBLUE);

        const char* lbl = TextFormat("%d", i);
        int tw = MeasureText(lbl, 22);
        DrawText(lbl, (int)(pos[i].x - (float)tw / 2.0f), (int)(pos[i].y - 11.0f), 22, BLACK);
    }
}

static void draw_button(Rectangle btn, int is_playing, int finished)
{
    Color color = is_playing ? RED : GREEN;
    const char* text = is_playing ? "Stop" : "Play";

    if (finished) {
        color = GRAY;
        text = "Done";
    }

    DrawRectangleRec(btn, color);
    DrawRectangleLines((int)btn.x, (int)btn.y, (int)btn.width, (int)btn.height, BLACK);

    int tw = MeasureText(text, 24);
    DrawText(text, (int)(btn.x + btn.width / 2.0f - (float)tw / 2.0f), (int)(btn.y + 10.0f), 24, WHITE);
}

static int has_ipc_travelers(const TravelerState* travelers, int num_travelers)
{
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].pipe_fd >= 0) return 1;
    }
    return 0;
}

static void stop_child(pid_t pid)
{
    if (pid > 0) kill(pid, SIGSTOP);
}

static void continue_child(pid_t pid)
{
    if (pid > 0) kill(pid, SIGCONT);
}

static void stop_all_children(TravelerState* travelers, int num_travelers)
{
    for (int i = 0; i < num_travelers; i++) {
        stop_child(travelers[i].pid);
    }
}

static void terminate_and_wait_children(TravelerState* travelers, int num_travelers)
{
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].pipe_fd >= 0) {
            close(travelers[i].pipe_fd);
            travelers[i].pipe_fd = -1;
        }

        if (travelers[i].pid > 0) {
            int status;
            pid_t done = waitpid(travelers[i].pid, &status, WNOHANG);
            if (done == 0) {
                kill(travelers[i].pid, SIGTERM);
                kill(travelers[i].pid, SIGCONT);
                waitpid(travelers[i].pid, NULL, 0);
            }
        }
    }
}

static int read_one_message(int fd, TravelerMsg* msg)
{
    if (fd < 0 || !msg) return 0;

    fd_set fds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int ready = select(fd + 1, &fds, NULL, NULL, &tv);
    if (ready <= 0) return 0;
    if (!FD_ISSET(fd, &fds)) return 0;

    ssize_t n = read(fd, msg, sizeof(*msg));
    if (n == 0) return -1;
    if (n != (ssize_t)sizeof(*msg)) return 0;

    return 1;
}

static void log_traveler_msg(const TravelerMsg* msg)
{
    if (!msg) return;

    if (msg->is_done) {
        printf("[PID=%d] arrived at node %d | DESTINATION\n", (int)msg->pid, msg->current_node);
        fflush(stdout);
        printf("[PID=%d] finished\n", (int)msg->pid);
        fflush(stdout);
    } else {
        printf("[PID=%d] arrived at node %d | next node: %d\n",
               (int)msg->pid,
               msg->current_node,
               msg->next_node);
        fflush(stdout);
    }
}

void show_graph_static(const Graph* graph)
{
    if (!graph) return;

    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Display");
    SetTargetFPS(60);

    Vector2 pos[MAX_NODES];

    if (graph->num_nodes > MAX_NODES) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Error: GUI supports up to 15 nodes", 250, 320, 28, RED);
            EndDrawing();
        }
        CloseWindow();
        return;
    }

    for (int i = 0; i < graph->num_nodes; i++) {
        pos[i] = get_node_position(i, graph->num_nodes);
    }

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Graph Display - Milestone 2", 30, 25, 28, BLACK);
        DrawText("Static directed weighted graph", 30, 65, 22, DARKGRAY);

        draw_edges(graph, pos);
        draw_nodes(graph, pos);

        EndDrawing();
    }

    CloseWindow();
}

void show_graph_animation(const Graph* graph, const int* path, int path_length)
{
    if (!graph) return;

    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Simulation");
    SetTargetFPS(60);

    Vector2 pos[MAX_NODES];

    if (graph->num_nodes > MAX_NODES) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Error: GUI supports up to 15 nodes", 250, 320, 28, RED);
            EndDrawing();
        }
        CloseWindow();
        return;
    }

    for (int i = 0; i < graph->num_nodes; i++) {
        pos[i] = get_node_position(i, graph->num_nodes);
    }

    Rectangle play_btn;
    play_btn.x = 30;
    play_btn.y = SCREEN_HEIGHT - 80;
    play_btn.width = 120;
    play_btn.height = 45;

    int is_playing = 0;
    int finished = 0;
    int cur_idx = 0;
    int cur_step = 0;
    float jump_timer = 0.0f;
    float wait_timer = 0.0f;
    int waiting = 0;

    if (!path || path_length <= 1) finished = 1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(GetMousePosition(), play_btn) && !finished) {
                is_playing = !is_playing;
            }
        }

        if (is_playing && !finished && path && path_length >= 2) {
            if (waiting) {
                wait_timer += dt;
                if (wait_timer >= NODE_WAIT_TIME) {
                    waiting = 0;
                    wait_timer = 0.0f;
                }
            } else {
                int edge_weight = get_edge_weight(graph, path[cur_idx], path[cur_idx + 1]);
                jump_timer += dt;

                if (jump_timer >= JUMP_TIME) {
                    jump_timer = 0.0f;
                    cur_step++;

                    if (cur_step >= edge_weight) {
                        cur_step = 0;
                        cur_idx++;

                        if (cur_idx >= path_length - 1) {
                            finished = 1;
                            is_playing = 0;
                        } else {
                            waiting = 1;
                        }
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Graph Simulation - Milestone 3", 30, 25, 28, BLACK);
        if (!path || path_length == 0) {
            DrawText("No path found", 30, 65, 24, RED);
        } else {
            DrawText("Shortest path is highlighted in orange", 30, 65, 22, DARKGRAY);
        }

        draw_edges(graph, pos);
        draw_path_highlight(graph, path, path_length, pos, ORANGE);
        draw_nodes(graph, pos);

        if (path && path_length > 0) {
            Vector2 heart_pos;

            if (path_length == 1 || finished) {
                heart_pos = pos[path[path_length - 1]];
            } else if (waiting) {
                heart_pos = pos[path[cur_idx]];
            } else {
                int edge_weight = get_edge_weight(graph, path[cur_idx], path[cur_idx + 1]);
                float prog = jump_timer / JUMP_TIME;
                if (prog > 1.0f) prog = 1.0f;

                float t = ((float)cur_step + prog) / (float)edge_weight;
                if (t > 1.0f) t = 1.0f;

                heart_pos = vector_lerp(pos[path[cur_idx]], pos[path[cur_idx + 1]], t);
            }

            draw_heart(heart_pos, 21.0f, RED, MAROON);
        }

        draw_button(play_btn, is_playing, finished);

        if (waiting) {
            DrawText("Waiting 1 second at node...", 180, SCREEN_HEIGHT - 70, 22, DARKBLUE);
        }

        if (finished && path && path_length > 0) {
            DrawText("Arrived at destination!", 180, SCREEN_HEIGHT - 70, 24, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();
}

static void update_ipc_traveler(const Graph* graph,
                                TravelerState* traveler,
                                IpcAnimState* anim,
                                float dt,
                                int is_playing)
{
    if (!graph || !traveler || !anim || anim->finished || !is_playing) return;

    if (anim->moving) {
        anim->move_timer += dt;
        if (anim->move_timer >= anim->move_duration) {
            anim->move_timer = anim->move_duration;
            anim->current_node = anim->next_node;
            anim->next_node = -1;
            anim->moving = 0;
            anim->waiting_at_node = 1;
            anim->wait_timer = 0.0f;
            anim->has_position = 1;
        }
        return;
    }

    if (anim->waiting_at_node) {
        anim->wait_timer += dt;
        if (anim->wait_timer >= NODE_WAIT_TIME) {
            anim->waiting_at_node = 0;
            anim->wait_timer = 0.0f;
            anim->waiting_for_msg = 0;
        }
        return;
    }

    TravelerMsg msg;
    int read_status = read_one_message(traveler->pipe_fd, &msg);

    if (read_status == 1) {
        stop_child(traveler->pid);
        anim->waiting_for_msg = 0;

        log_traveler_msg(&msg);

        anim->current_node = msg.current_node;
        anim->next_node = msg.next_node;
        anim->has_position = 1;

        if (msg.is_done) {
            anim->finished = 1;
            anim->moving = 0;
            anim->waiting_at_node = 0;
            if (traveler->pipe_fd >= 0) {
                close(traveler->pipe_fd);
                traveler->pipe_fd = -1;
            }
            return;
        }

        if (msg.next_node >= 0) {
            int edge_weight = get_edge_weight(graph, msg.current_node, msg.next_node);
            anim->move_duration = (float)edge_weight * EDGE_UNIT_TIME;
            if (anim->move_duration < 0.15f) anim->move_duration = 0.15f;
            anim->move_timer = 0.0f;
            anim->moving = 1;
        }

        return;
    }

    if (read_status == -1) {
        anim->finished = 1;
        if (traveler->pipe_fd >= 0) {
            close(traveler->pipe_fd);
            traveler->pipe_fd = -1;
        }
        return;
    }

    if (!anim->waiting_for_msg) {
        continue_child(traveler->pid);
        anim->waiting_for_msg = 1;
    }
}

static Vector2 get_ipc_traveler_position(const TravelerState* traveler,
                                         const IpcAnimState* anim,
                                         Vector2 pos[])
{
    if (anim->has_position) {
        if (anim->moving && anim->next_node >= 0) {
            float t = anim->move_timer / anim->move_duration;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            return vector_lerp(pos[anim->current_node], pos[anim->next_node], t);
        }

        return pos[anim->current_node];
    }

    if (traveler->path && traveler->path_length > 0) {
        return pos[traveler->path[0]];
    }

    return pos[0];
}

static void show_graph_multi_animation_ipc(const Graph* graph,
                                           TravelerState* travelers,
                                           int num_travelers)
{
    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Simulation - Milestone 5");
    SetTargetFPS(60);

    Vector2 pos[MAX_NODES];

    if (graph->num_nodes > MAX_NODES) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Error: GUI supports up to 15 nodes", 250, 320, 28, RED);
            EndDrawing();
        }
        CloseWindow();
        terminate_and_wait_children(travelers, num_travelers);
        return;
    }

    for (int i = 0; i < graph->num_nodes; i++) {
        pos[i] = get_node_position(i, graph->num_nodes);
    }

    if (num_travelers > MAX_TRAVELERS) num_travelers = MAX_TRAVELERS;

    IpcAnimState anim[MAX_TRAVELERS];
    for (int t = 0; t < num_travelers; t++) {
        anim[t].has_position = 0;
        anim[t].current_node = -1;
        anim[t].next_node = -1;
        anim[t].moving = 0;
        anim[t].move_timer = 0.0f;
        anim[t].move_duration = 1.0f;
        anim[t].waiting_at_node = 0;
        anim[t].wait_timer = 0.0f;
        anim[t].waiting_for_msg = 0;
        anim[t].finished = 0;
    }

    stop_all_children(travelers, num_travelers);

    Rectangle play_btn;
    play_btn.x = 30;
    play_btn.y = SCREEN_HEIGHT - 80;
    play_btn.width = 120;
    play_btn.height = 45;

    int is_playing = 0;
    int all_done = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(GetMousePosition(), play_btn) && !all_done) {
                is_playing = !is_playing;
                if (!is_playing) {
                    stop_all_children(travelers, num_travelers);
                }
            }
        }

        if (is_playing) {
            for (int t = 0; t < num_travelers; t++) {
                update_ipc_traveler(graph, &travelers[t], &anim[t], dt, is_playing);
            }
        }

        all_done = 1;
        for (int t = 0; t < num_travelers; t++) {
            if (!anim[t].finished) {
                all_done = 0;
                break;
            }
        }
        if (all_done) is_playing = 0;

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Graph Simulation - Milestone 5", 30, 25, 28, BLACK);
        DrawText("IPC mode: GUI position is updated from child messages", 30, 58, 20, DARKGRAY);

        draw_edges(graph, pos);

        for (int t = 0; t < num_travelers; t++) {
            if (anim[t].has_position && anim[t].next_node >= 0 && anim[t].moving) {
                draw_arrow(pos[anim[t].current_node], pos[anim[t].next_node], TRAVELER_COLORS[t], 5.0f);
                draw_edge_weight(pos[anim[t].current_node], pos[anim[t].next_node],
                                 get_edge_weight(graph, anim[t].current_node, anim[t].next_node));
            }
        }

        draw_nodes(graph, pos);

        for (int t = 0; t < num_travelers; t++) {
            Vector2 heart_pos = get_ipc_traveler_position(&travelers[t], &anim[t], pos);
            heart_pos.x += (float)t * 3.0f;
            heart_pos.y += (float)t * 2.0f;

            Color outline = Fade(TRAVELER_COLORS[t], anim[t].finished ? 0.35f : 0.85f);
            Color fill = HEART_FILL_COLORS[t];
            if (anim[t].finished) fill = Fade(fill, 0.35f);

            draw_heart(heart_pos, 18.0f, fill, outline);
        }

        for (int t = 0; t < num_travelers; t++) {
            int lx = 30 + t * 115;
            int ly = SCREEN_HEIGHT - 120;

            DrawRectangle(lx, ly, 18, 18, TRAVELER_COLORS[t]);
            DrawRectangleLines(lx, ly, 18, 18, BLACK);

            const char* lbl;
            if (anim[t].finished) {
                lbl = TextFormat("T%d done", t);
            } else if (!is_playing) {
                lbl = TextFormat("T%d paused", t);
            } else if (anim[t].moving) {
                lbl = TextFormat("T%d moving", t);
            } else if (anim[t].waiting_at_node) {
                lbl = TextFormat("T%d wait", t);
            } else {
                lbl = TextFormat("T%d ready", t);
            }

            DrawText(lbl, lx + 22, ly + 1, 18, DARKGRAY);
        }

        draw_button(play_btn, is_playing, all_done);

        if (all_done) {
            DrawText("All travelers arrived!", 170, SCREEN_HEIGHT - 70, 24, DARKGREEN);
        } else if (!is_playing) {
            DrawText("Press Play to resume child processes", 170, SCREEN_HEIGHT - 70, 22, DARKBLUE);
        } else {
            DrawText("Running from child IPC updates", 170, SCREEN_HEIGHT - 70, 22, DARKBLUE);
        }

        EndDrawing();
    }

    CloseWindow();
    terminate_and_wait_children(travelers, num_travelers);
}

static void show_graph_multi_animation_parent(const Graph* graph,
                                              TravelerState* travelers,
                                              int num_travelers)
{
    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Simulation");
    SetTargetFPS(60);

    Vector2 pos[MAX_NODES];

    if (graph->num_nodes > MAX_NODES) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Error: GUI supports up to 15 nodes", 250, 320, 28, RED);
            EndDrawing();
        }
        CloseWindow();
        terminate_and_wait_children(travelers, num_travelers);
        return;
    }

    for (int i = 0; i < graph->num_nodes; i++) {
        pos[i] = get_node_position(i, graph->num_nodes);
    }

    if (num_travelers > MAX_TRAVELERS) num_travelers = MAX_TRAVELERS;

    PathAnimState anim[MAX_TRAVELERS];
    for (int t = 0; t < num_travelers; t++) {
        anim[t].current_path_index = 0;
        anim[t].current_step = 0;
        anim[t].jump_timer = 0.0f;
        anim[t].wait_timer = 0.0f;
        anim[t].waiting_at_node = 0;
        anim[t].signal_sent = 0;
        anim[t].finished = (travelers[t].path == NULL || travelers[t].path_length <= 1) ? 1 : 0;
    }

    Rectangle play_btn;
    play_btn.x = 30;
    play_btn.y = SCREEN_HEIGHT - 80;
    play_btn.width = 120;
    play_btn.height = 45;

    int is_playing = 0;
    int all_done = 1;

    for (int t = 0; t < num_travelers; t++) {
        if (!anim[t].finished) {
            all_done = 0;
            break;
        }
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(GetMousePosition(), play_btn) && !all_done) {
                is_playing = !is_playing;
            }
        }

        if (is_playing) {
            for (int t = 0; t < num_travelers; t++) {
                if (anim[t].finished) continue;

                const int* path = travelers[t].path;
                int path_len = travelers[t].path_length;

                if (anim[t].waiting_at_node) {
                    anim[t].wait_timer += dt;
                    if (anim[t].wait_timer >= NODE_WAIT_TIME) {
                        anim[t].waiting_at_node = 0;
                        anim[t].wait_timer = 0.0f;
                    }
                } else {
                    int src = path[anim[t].current_path_index];
                    int dst = path[anim[t].current_path_index + 1];
                    int edge_weight = get_edge_weight(graph, src, dst);

                    anim[t].jump_timer += dt;
                    if (anim[t].jump_timer >= JUMP_TIME) {
                        anim[t].jump_timer = 0.0f;
                        anim[t].current_step++;

                        if (anim[t].current_step >= edge_weight) {
                            anim[t].current_step = 0;
                            anim[t].current_path_index++;

                            if (anim[t].current_path_index >= path_len - 1) {
                                anim[t].finished = 1;
                                if (!anim[t].signal_sent && travelers[t].pid > 0) {
                                    kill(travelers[t].pid, SIGTERM);
                                    anim[t].signal_sent = 1;
                                }
                            } else {
                                anim[t].waiting_at_node = 1;
                            }
                        }
                    }
                }
            }

            all_done = 1;
            for (int t = 0; t < num_travelers; t++) {
                if (!anim[t].finished) {
                    all_done = 0;
                    break;
                }
            }

            if (all_done) is_playing = 0;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Graph Simulation - Milestone 4", 30, 25, 28, BLACK);
        DrawText("Parent-controlled multiple travelers", 30, 58, 20, DARKGRAY);

        draw_edges(graph, pos);

        for (int t = 0; t < num_travelers; t++) {
            Color color = TRAVELER_COLORS[t];
            if (anim[t].finished) color = Fade(color, 0.35f);
            draw_path_highlight(graph, travelers[t].path, travelers[t].path_length, pos, color);
        }

        draw_nodes(graph, pos);

        for (int t = 0; t < num_travelers; t++) {
            const int* path = travelers[t].path;
            int path_len = travelers[t].path_length;
            if (!path || path_len == 0) continue;

            Vector2 heart_pos;

            if (path_len == 1 || anim[t].finished) {
                heart_pos = pos[path[path_len - 1]];
            } else if (anim[t].waiting_at_node) {
                heart_pos = pos[path[anim[t].current_path_index]];
            } else {
                int src = path[anim[t].current_path_index];
                int dst = path[anim[t].current_path_index + 1];
                int edge_weight = get_edge_weight(graph, src, dst);

                float prog = anim[t].jump_timer / JUMP_TIME;
                if (prog > 1.0f) prog = 1.0f;

                float tt = ((float)anim[t].current_step + prog) / (float)edge_weight;
                if (tt > 1.0f) tt = 1.0f;

                heart_pos = vector_lerp(pos[src], pos[dst], tt);
            }

            heart_pos.x += (float)t * 3.0f;
            heart_pos.y += (float)t * 2.0f;

            draw_heart(heart_pos, 18.0f, HEART_FILL_COLORS[t], Fade(TRAVELER_COLORS[t], 0.8f));
        }

        for (int t = 0; t < num_travelers; t++) {
            int lx = 30 + t * 110;
            int ly = SCREEN_HEIGHT - 120;

            DrawRectangle(lx, ly, 18, 18, TRAVELER_COLORS[t]);
            DrawRectangleLines(lx, ly, 18, 18, BLACK);

            const char* lbl;
            if (anim[t].finished) {
                lbl = TextFormat("T%d done", t);
            } else if (!is_playing) {
                lbl = TextFormat("T%d ready", t);
            } else if (travelers[t].path == NULL) {
                lbl = TextFormat("T%d no path", t);
            } else {
                lbl = TextFormat("T%d going", t);
            }

            DrawText(lbl, lx + 22, ly + 1, 18, DARKGRAY);
        }

        draw_button(play_btn, is_playing, all_done);

        if (all_done) {
            DrawText("All travelers arrived!", 170, SCREEN_HEIGHT - 70, 24, DARKGREEN);
        }

        EndDrawing();
    }

    CloseWindow();
    terminate_and_wait_children(travelers, num_travelers);
}

void show_graph_multi_animation(const Graph* graph,
                                TravelerState* travelers,
                                int num_travelers)
{
    if (!graph || !travelers || num_travelers <= 0) return;

    if (has_ipc_travelers(travelers, num_travelers)) {
        show_graph_multi_animation_ipc(graph, travelers, num_travelers);
    } else {
        show_graph_multi_animation_parent(graph, travelers, num_travelers);
    }
}
