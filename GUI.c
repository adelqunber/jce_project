#define _POSIX_C_SOURCE 200809L
#include "GUI.h"
#include "raylib.h"

#include <math.h>
#include <stddef.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include "IPC.h"

#define SCREEN_WIDTH  1000
#define SCREEN_HEIGHT 700
#define MAX_NODES     15

#define NODE_RADIUS  25.0f
#define ARROW_SIZE   12.0f

#define JUMP_TIME      0.3f
#define NODE_WAIT_TIME 1.0f

#define MAX_TRAVELERS 8

typedef struct {
    int   current_path_index;
    int   current_step;
    float jump_timer;
    float wait_timer;
    int   waiting_at_node;
    int   finished;
    int   signal_sent;
} AnimState;

static const Color TRAVELER_COLORS[MAX_TRAVELERS] = {
    RED,
    BLUE,
    GREEN,
    PURPLE,
    ORANGE,
    PINK,
    LIME,
    GOLD
};

static const Color HEART_FILL_COLORS[MAX_TRAVELERS] = {
    { 200,  50,  50, 255 },
    {  50,  80, 200, 255 },
    {  50, 160,  50, 255 },
    { 140,  50, 180, 255 },
    { 220, 130,  20, 255 },
    { 220,  90, 160, 255 },
    { 140, 200,  20, 255 },
    { 200, 170,   0, 255 },
};

static Vector2 get_node_position(int node, int total_nodes) {
    float cx = SCREEN_WIDTH  / 2.0f;
    float cy = SCREEN_HEIGHT / 2.0f;
    float r  = 240.0f;
    float angle = (2.0f * PI * node) / total_nodes - PI / 2.0f;
    return (Vector2){ cx + r * cosf(angle), cy + r * sinf(angle) };
}

static Vector2 vector_lerp(Vector2 a, Vector2 b, float t) {
    return (Vector2){ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

static int get_edge_weight(const Graph* graph, int src, int dst) {
    for (Edge* e = graph->adj[src]; e != NULL; e = e->next) {
        if (e->dst == dst) return (e->weight > 0) ? e->weight : 1;
    }
    return 1;
}

static void draw_heart(Vector2 center, float size, Color fill, Color outline) {
    int limit = (int)(size * 1.4f);
    for (int y = -limit; y <= limit; y++) {
        for (int x = -limit; x <= limit; x++) {
            float nx =  (float)x / size;
            float ny = -(float)y / size;
            float v  = powf(nx*nx + ny*ny - 1.0f, 3.0f) - nx*nx*ny*ny*ny;
            if (v <= 0.0f)
                DrawPixel((int)(center.x + x), (int)(center.y + y), fill);
        }
    }

    const int SEG = 80;
    Vector2 pts[SEG + 1];
    float scale = size / 21.0f;
    for (int i = 0; i <= SEG; i++) {
        float t = (2.0f * PI * i) / SEG;
        pts[i].x = center.x + 16.0f * powf(sinf(t), 3.0f) * scale;
        pts[i].y = center.y - (13.0f*cosf(t) - 5.0f*cosf(2*t)
                                - 2.0f*cosf(3*t) - cosf(4*t)) * scale;
    }
    DrawLineStrip(pts, SEG + 1, outline);
}

static void draw_arrow(Vector2 start, Vector2 end, Color color, float thickness) {
    Vector2 d = { end.x - start.x, end.y - start.y };
    float len = sqrtf(d.x*d.x + d.y*d.y);
    if (len == 0) return;
    d.x /= len; d.y /= len;

    Vector2 ls = { start.x + d.x * NODE_RADIUS, start.y + d.y * NODE_RADIUS };
    Vector2 le = { end.x   - d.x * NODE_RADIUS, end.y   - d.y * NODE_RADIUS };
    DrawLineEx(ls, le, thickness, color);

    float angle = atan2f(d.y, d.x);
    Vector2 p1 = { le.x - ARROW_SIZE * cosf(angle - PI/6.0f),
                   le.y - ARROW_SIZE * sinf(angle - PI/6.0f) };
    Vector2 p2 = { le.x - ARROW_SIZE * cosf(angle + PI/6.0f),
                   le.y - ARROW_SIZE * sinf(angle + PI/6.0f) };
    DrawTriangle(le, p1, p2, color);
}

static void draw_edge_weight(Vector2 start, Vector2 end, int weight) {
    Vector2 mid = { (start.x + end.x) / 2.0f, (start.y + end.y) / 2.0f };
    const char* text = TextFormat("%d", weight);
    int tw = MeasureText(text, 20);
    DrawRectangle((int)(mid.x - tw/2 - 5), (int)(mid.y - 12), tw + 10, 24, RAYWHITE);
    DrawText(text, (int)(mid.x - tw/2), (int)(mid.y - 10), 20, RED);
}

static void draw_edges(const Graph* graph, Vector2 pos[]) {
    for (int src = 0; src < graph->num_nodes; src++) {
        for (Edge* e = graph->adj[src]; e != NULL; e = e->next) {
            draw_arrow(pos[src], pos[e->dst], DARKGRAY, 3.0f);
            draw_edge_weight(pos[src], pos[e->dst], e->weight);
        }
    }
}

static void draw_path_highlight(
    const Graph* graph, const int* path, int path_length,
    Vector2 pos[], Color color
) {
    if (!path || path_length < 2) return;
    for (int i = 0; i < path_length - 1; i++) {
        draw_arrow(pos[path[i]], pos[path[i+1]], color, 5.0f);
        draw_edge_weight(pos[path[i]], pos[path[i+1]],
                         get_edge_weight(graph, path[i], path[i+1]));
    }
}

static void draw_nodes(const Graph* graph, Vector2 pos[]) {
    for (int i = 0; i < graph->num_nodes; i++) {
        DrawCircleV(pos[i], NODE_RADIUS, SKYBLUE);
        DrawCircleLines((int)pos[i].x, (int)pos[i].y, NODE_RADIUS, DARKBLUE);
        const char* lbl = TextFormat("%d", i);
        int tw = MeasureText(lbl, 22);
        DrawText(lbl, (int)(pos[i].x - tw/2), (int)(pos[i].y - 11), 22, BLACK);
    }
}

static void draw_button(Rectangle btn, int is_playing, int finished) {
    Color c = is_playing ? RED : GREEN;
    const char* txt = is_playing ? "Stop" : "Play";
    if (finished) { c = GRAY; txt = "Done"; }
    DrawRectangleRec(btn, c);
    DrawRectangleLines((int)btn.x, (int)btn.y, (int)btn.width, (int)btn.height, BLACK);
    int tw = MeasureText(txt, 24);
    DrawText(txt, (int)(btn.x + btn.width/2 - tw/2), (int)(btn.y + 10), 24, WHITE);
}

void show_graph_static(const Graph* graph) {
    if (!graph) return;
    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Display");
    SetTargetFPS(60);

    Vector2 pos[MAX_NODES];
    if (graph->num_nodes > MAX_NODES) {
        while (!WindowShouldClose()) {
            BeginDrawing(); ClearBackground(RAYWHITE);
            DrawText("Error: GUI supports up to 15 nodes", 250, 320, 28, RED);
            EndDrawing();
        }
        CloseWindow(); return;
    }
    for (int i = 0; i < graph->num_nodes; i++)
        pos[i] = get_node_position(i, graph->num_nodes);

    while (!WindowShouldClose()) {
        BeginDrawing(); ClearBackground(RAYWHITE);
        DrawText("Graph Display - Milestone 2", 30, 25, 28, BLACK);
        DrawText("Static directed weighted graph", 30, 65, 22, DARKGRAY);
        draw_edges(graph, pos);
        draw_nodes(graph, pos);
        EndDrawing();
    }
    CloseWindow();
}

void show_graph_animation(const Graph* graph, const int* path, int path_length) {
    if (!graph) return;
    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Simulation");
    SetTargetFPS(60);

    Vector2 pos[MAX_NODES];
    if (graph->num_nodes > MAX_NODES) {
        while (!WindowShouldClose()) {
            BeginDrawing(); ClearBackground(RAYWHITE);
            DrawText("Error: GUI supports up to 15 nodes", 250, 320, 28, RED);
            EndDrawing();
        }
        CloseWindow(); return;
    }
    for (int i = 0; i < graph->num_nodes; i++)
        pos[i] = get_node_position(i, graph->num_nodes);

    Rectangle play_btn = { 30, SCREEN_HEIGHT - 80, 120, 45 };
    int is_playing = 0, finished = 0;
    int cur_idx = 0, cur_step = 0;
    float jump_timer = 0, wait_timer = 0;
    int waiting = 0;

    if (!path || path_length <= 1) finished = 1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(GetMousePosition(), play_btn) && !finished)
                is_playing = !is_playing;
        }

        if (is_playing && !finished && path && path_length >= 2) {
            if (waiting) {
                wait_timer += dt;
                if (wait_timer >= NODE_WAIT_TIME) { waiting = 0; wait_timer = 0; }
            } else {
                int ew = get_edge_weight(graph, path[cur_idx], path[cur_idx+1]);
                jump_timer += dt;
                if (jump_timer >= JUMP_TIME) {
                    jump_timer = 0; cur_step++;
                    if (cur_step >= ew) {
                        cur_step = 0; cur_idx++;
                        if (cur_idx >= path_length - 1) { finished = 1; is_playing = 0; }
                        else waiting = 1;
                    }
                }
            }
        }

        BeginDrawing(); ClearBackground(RAYWHITE);
        DrawText("Graph Simulation - Milestone 3", 30, 25, 28, BLACK);
        if (!path || path_length == 0)
            DrawText("No path found", 30, 65, 24, RED);
        else
            DrawText("Shortest path is highlighted in orange", 30, 65, 22, DARKGRAY);

        draw_edges(graph, pos);
        draw_path_highlight(graph, path, path_length, pos, ORANGE);
        draw_nodes(graph, pos);

        if (path && path_length > 0) {
            Vector2 ep;
            if (path_length == 1 || finished)
                ep = pos[path[path_length-1]];
            else if (waiting)
                ep = pos[path[cur_idx]];
            else {
                int ew = get_edge_weight(graph, path[cur_idx], path[cur_idx+1]);
                float prog = jump_timer / JUMP_TIME;
                if (prog > 1) prog = 1;
                float t = ((float)cur_step + prog) / (float)ew;
                if (t > 1) t = 1;
                ep = vector_lerp(pos[path[cur_idx]], pos[path[cur_idx+1]], t);
            }
            draw_heart(ep, 21.0f, RED, MAROON);
        }

        draw_button(play_btn, is_playing, finished);

        if (waiting)
            DrawText("Waiting 1 second at node...", 180, SCREEN_HEIGHT - 70, 22, DARKBLUE);
        if (finished && path && path_length > 0)
            DrawText("Arrived at destination!", 180, SCREEN_HEIGHT - 70, 24, GREEN);

        EndDrawing();
    }
    CloseWindow();
}


static void poll_log_messages(TravelerState* travelers, int num_travelers) {
    for (int t = 0; t < num_travelers; t++) {
        if (travelers[t].pipe_fd < 0) continue;

        fd_set fds;
        struct timeval tv = { 0, 0 };
        FD_ZERO(&fds);
        FD_SET(travelers[t].pipe_fd, &fds);

        if (select(travelers[t].pipe_fd + 1, &fds, NULL, NULL, &tv) <= 0)
            continue;
        if (!FD_ISSET(travelers[t].pipe_fd, &fds))
            continue;

        TravelerMsg msg;
        ssize_t n = read(travelers[t].pipe_fd, &msg, sizeof(msg));
        if (n != (ssize_t)sizeof(msg)) continue;

        if (msg.is_done) {
            printf("[PID=%d] arrived at node %d | DESTINATION\n",
                   (int)msg.pid, msg.current_node);
            fflush(stdout);
            printf("[PID=%d] finished\n", (int)msg.pid);
            fflush(stdout);
            close(travelers[t].pipe_fd);
            travelers[t].pipe_fd = -1;
        } else {
            printf("[PID=%d] arrived at node %d | next node: %d\n",
                   (int)msg.pid, msg.current_node, msg.next_node);
            fflush(stdout);
        }
    }
}

void show_graph_multi_animation(
    const Graph*   graph,
    TravelerState* travelers,
    int            num_travelers
) {
    if (!graph || !travelers || num_travelers <= 0) return;

    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Simulation");
    SetTargetFPS(60);

    Vector2 pos[MAX_NODES];
    if (graph->num_nodes > MAX_NODES) {
        while (!WindowShouldClose()) {
            BeginDrawing(); ClearBackground(RAYWHITE);
            DrawText("Error: GUI supports up to 15 nodes", 250, 320, 28, RED);
            EndDrawing();
        }
        CloseWindow(); return;
    }
    for (int i = 0; i < graph->num_nodes; i++)
        pos[i] = get_node_position(i, graph->num_nodes);

    if (num_travelers > MAX_TRAVELERS) num_travelers = MAX_TRAVELERS;

    AnimState anim[MAX_TRAVELERS];
    for (int t = 0; t < num_travelers; t++) {
        anim[t].current_path_index = 0;
        anim[t].current_step       = 0;
        anim[t].jump_timer         = 0;
        anim[t].wait_timer         = 0;
        anim[t].waiting_at_node    = 0;
        anim[t].signal_sent        = 0;
        anim[t].finished = (travelers[t].path == NULL
                            || travelers[t].path_length <= 1) ? 1 : 0;
    }

    Rectangle play_btn = { 30, SCREEN_HEIGHT - 80, 120, 45 };
    int is_playing = 0;

    int all_done = 1;
    for (int t = 0; t < num_travelers; t++)
        if (!anim[t].finished) { all_done = 0; break; }

    for (int t = 0; t < num_travelers; t++) {
        if (anim[t].finished && !anim[t].signal_sent && travelers[t].pid > 0) {
            kill(travelers[t].pid, SIGTERM);
            anim[t].signal_sent = 1;
        }
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(GetMousePosition(), play_btn) && !all_done)
                is_playing = !is_playing;
        }

        if (is_playing) {

            poll_log_messages(travelers, num_travelers);

            all_done = 1;
            for (int t = 0; t < num_travelers; t++) {
                if (anim[t].finished) continue;
                all_done = 0;

                const int* path     = travelers[t].path;
                int        path_len = travelers[t].path_length;

                if (anim[t].waiting_at_node) {
                    anim[t].wait_timer += dt;
                    if (anim[t].wait_timer >= NODE_WAIT_TIME) {
                        anim[t].waiting_at_node = 0;
                        anim[t].wait_timer      = 0;
                    }
                } else {
                    int src = path[anim[t].current_path_index];
                    int dst = path[anim[t].current_path_index + 1];
                    int ew  = get_edge_weight(graph, src, dst);

                    anim[t].jump_timer += dt;
                    if (anim[t].jump_timer >= JUMP_TIME) {
                        anim[t].jump_timer = 0;
                        anim[t].current_step++;
                        if (anim[t].current_step >= ew) {
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
            for (int t = 0; t < num_travelers; t++)
                if (!anim[t].finished) { all_done = 0; break; }
            if (all_done) is_playing = 0;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Graph Simulation", 30, 25, 28, BLACK);
        DrawText("Multiple travelers (each color = one process)", 30, 58, 20, DARKGRAY);

        draw_edges(graph, pos);

        for (int t = 0; t < num_travelers; t++) {
            Color c = TRAVELER_COLORS[t];
            if (anim[t].finished) c = Fade(c, 0.35f);
            draw_path_highlight(graph, travelers[t].path,
                                travelers[t].path_length, pos, c);
        }

        draw_nodes(graph, pos);

        for (int t = 0; t < num_travelers; t++) {
            const int* path = travelers[t].path;
            int        plen = travelers[t].path_length;
            if (!path || plen == 0) continue;

            Vector2 ep;
            if (plen == 1 || anim[t].finished) {
                ep = pos[path[plen - 1]];
            } else if (anim[t].waiting_at_node) {
                ep = pos[path[anim[t].current_path_index]];
            } else {
                int src = path[anim[t].current_path_index];
                int dst = path[anim[t].current_path_index + 1];
                int ew  = get_edge_weight(graph, src, dst);
                float prog = anim[t].jump_timer / JUMP_TIME;
                if (prog > 1) prog = 1;
                float tt = ((float)anim[t].current_step + prog) / (float)ew;
                if (tt > 1) tt = 1;
                ep = vector_lerp(pos[src], pos[dst], tt);
            }

            ep.x += t * 3.0f;
            ep.y += t * 2.0f;

            draw_heart(ep, 18.0f, HEART_FILL_COLORS[t], Fade(TRAVELER_COLORS[t], 0.8f));
        }

        for (int t = 0; t < num_travelers; t++) {
            int lx = 30 + t * 110;
            int ly = SCREEN_HEIGHT - 120;
            DrawRectangle(lx, ly, 18, 18, TRAVELER_COLORS[t]);
            DrawRectangleLines(lx, ly, 18, 18, BLACK);
            const char* lbl;
            if (anim[t].finished)
                lbl = TextFormat("T%d done", t);
            else if (!is_playing)
                lbl = TextFormat("T%d ready", t);
            else if (travelers[t].path == NULL)
                lbl = TextFormat("T%d no path", t);
            else
                lbl = TextFormat("T%d going", t);
            DrawText(lbl, lx + 22, ly + 1, 18, DARKGRAY);
        }

        draw_button(play_btn, is_playing, all_done);

        if (all_done)
            DrawText("All travelers arrived!", 170, SCREEN_HEIGHT - 70, 24, DARKGREEN);

        EndDrawing();
    }

    CloseWindow();

    for (int t = 0; t < num_travelers; t++) {
        if (travelers[t].pipe_fd >= 0) {
            close(travelers[t].pipe_fd);
            travelers[t].pipe_fd = -1;
        }
        if (travelers[t].pid > 0) {
            if (!anim[t].signal_sent)
                kill(travelers[t].pid, SIGTERM);
            waitpid(travelers[t].pid, NULL, 0);
        }
    }
}