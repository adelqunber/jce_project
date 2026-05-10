#include "GUI.h"
#include "raylib.h"

#include <math.h>
#include <stddef.h>

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700

#define NODE_RADIUS 25.0f
#define ARROW_SIZE 12.0f

#define JUMP_TIME 0.3f
#define NODE_WAIT_TIME 1.0f

static Vector2 get_node_position(int node, int total_nodes) {
    float center_x = SCREEN_WIDTH / 2.0f;
    float center_y = SCREEN_HEIGHT / 2.0f;
    float radius = 240.0f;

    float angle = (2.0f * PI * node) / total_nodes - PI / 2.0f;

    Vector2 position;
    position.x = center_x + radius * cosf(angle);
    position.y = center_y + radius * sinf(angle);

    return position;
}

static Vector2 vector_lerp(Vector2 start, Vector2 end, float t) {
    Vector2 result;
    result.x = start.x + (end.x - start.x) * t;
    result.y = start.y + (end.y - start.y) * t;
    return result;
}

static int get_edge_weight(const Graph* graph, int src, int dst) {
    Edge* edge = graph->adj[src];

    while (edge != NULL) {
        if (edge->dst == dst) {
            if (edge->weight <= 0) {
                return 1;
            }

            return edge->weight;
        }

        edge = edge->next;
    }

    return 1;
}

static void draw_heart(Vector2 center, float size) {
    int limit = (int)(size * 1.4f);

    for (int y = -limit; y <= limit; y++) {
        for (int x = -limit; x <= limit; x++) {
            float nx = (float)x / size;
            float ny = -(float)y / size;

            float value =
                powf(nx * nx + ny * ny - 1.0f, 3.0f)
                - nx * nx * ny * ny * ny;

            if (value <= 0.0f) {
                DrawPixel(
                    (int)(center.x + x),
                    (int)(center.y + y),
                    RED
                );
            }
        }
    }

    const int segments = 80;
    Vector2 outline[segments + 1];

    float scale = size / 21.0f;

    for (int i = 0; i <= segments; i++) {
        float t = (2.0f * PI * i) / segments;

        float x = 16.0f * powf(sinf(t), 3.0f);
        float y = -(
            13.0f * cosf(t)
            - 5.0f * cosf(2.0f * t)
            - 2.0f * cosf(3.0f * t)
            - cosf(4.0f * t)
        );

        outline[i].x = center.x + x * scale;
        outline[i].y = center.y + y * scale;
    }

    DrawLineStrip(outline, segments + 1, MAROON);
}

static void draw_arrow(Vector2 start, Vector2 end, Color color, float thickness) {
    Vector2 direction;
    direction.x = end.x - start.x;
    direction.y = end.y - start.y;

    float length = sqrtf(direction.x * direction.x + direction.y * direction.y);

    if (length == 0) {
        return;
    }

    direction.x /= length;
    direction.y /= length;

    Vector2 line_start;
    line_start.x = start.x + direction.x * NODE_RADIUS;
    line_start.y = start.y + direction.y * NODE_RADIUS;

    Vector2 line_end;
    line_end.x = end.x - direction.x * NODE_RADIUS;
    line_end.y = end.y - direction.y * NODE_RADIUS;

    DrawLineEx(line_start, line_end, thickness, color);

    float angle = atan2f(direction.y, direction.x);

    Vector2 arrow_point1;
    arrow_point1.x = line_end.x - ARROW_SIZE * cosf(angle - PI / 6.0f);
    arrow_point1.y = line_end.y - ARROW_SIZE * sinf(angle - PI / 6.0f);

    Vector2 arrow_point2;
    arrow_point2.x = line_end.x - ARROW_SIZE * cosf(angle + PI / 6.0f);
    arrow_point2.y = line_end.y - ARROW_SIZE * sinf(angle + PI / 6.0f);

    DrawTriangle(line_end, arrow_point1, arrow_point2, color);
}

static void draw_edge_weight(Vector2 start, Vector2 end, int weight) {
    Vector2 middle;
    middle.x = (start.x + end.x) / 2.0f;
    middle.y = (start.y + end.y) / 2.0f;

    const char* text = TextFormat("%d", weight);
    int text_width = MeasureText(text, 20);

    DrawRectangle(
        (int)(middle.x - text_width / 2 - 5),
        (int)(middle.y - 12),
        text_width + 10,
        24,
        RAYWHITE
    );

    DrawText(
        text,
        (int)(middle.x - text_width / 2),
        (int)(middle.y - 10),
        20,
        RED
    );
}

static void draw_edges(const Graph* graph, Vector2 positions[]) {
    for (int src = 0; src < graph->num_nodes; src++) {
        Edge* edge = graph->adj[src];

        while (edge != NULL) {
            int dst = edge->dst;

            draw_arrow(positions[src], positions[dst], DARKGRAY, 3.0f);
            draw_edge_weight(positions[src], positions[dst], edge->weight);

            edge = edge->next;
        }
    }
}

static void draw_path_highlight(
    const Graph* graph,
    const int* path,
    int path_length,
    Vector2 positions[]
) {
    if (path == NULL || path_length < 2) {
        return;
    }

    for (int i = 0; i < path_length - 1; i++) {
        int src = path[i];
        int dst = path[i + 1];

        draw_arrow(positions[src], positions[dst], ORANGE, 5.0f);
        draw_edge_weight(positions[src], positions[dst], get_edge_weight(graph, src, dst));
    }
}

static void draw_nodes(const Graph* graph, Vector2 positions[]) {
    for (int i = 0; i < graph->num_nodes; i++) {
        DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);

        DrawCircleLines(
            (int)positions[i].x,
            (int)positions[i].y,
            NODE_RADIUS,
            DARKBLUE
        );

        const char* label = TextFormat("%d", i);
        int text_width = MeasureText(label, 22);

        DrawText(
            label,
            (int)(positions[i].x - text_width / 2),
            (int)(positions[i].y - 11),
            22,
            BLACK
        );
    }
}

static void draw_button(Rectangle button, int is_playing, int finished) {
    Color button_color = is_playing ? RED : GREEN;
    const char* text = is_playing ? "Stop" : "Play";

    if (finished) {
        button_color = GRAY;
        text = "Done";
    }

    DrawRectangleRec(button, button_color);

    DrawRectangleLines(
        (int)button.x,
        (int)button.y,
        (int)button.width,
        (int)button.height,
        BLACK
    );

    int text_width = MeasureText(text, 24);

    DrawText(
        text,
        (int)(button.x + button.width / 2 - text_width / 2),
        (int)(button.y + 10),
        24,
        WHITE
    );
}

static void draw_path_text(const int* path, int path_length) {
    if (path == NULL || path_length == 0) {
        DrawText("No path found", 30, 65, 24, RED);
        return;
    }

    DrawText("Shortest path is highlighted in orange", 30, 65, 22, DARKGRAY);
}

void show_graph_static(const Graph* graph) {
    if (graph == NULL) {
        return;
    }

    SetTraceLogLevel(LOG_NONE);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Display");
    SetTargetFPS(60);

    Vector2 positions[15];

    if (graph->num_nodes > 15) {
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
        positions[i] = get_node_position(i, graph->num_nodes);
    }

    while (!WindowShouldClose()) {
        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Graph Display - Milestone 2", 30, 25, 28, BLACK);
        DrawText("Static directed weighted graph", 30, 65, 22, DARKGRAY);

        draw_edges(graph, positions);
        draw_nodes(graph, positions);

        EndDrawing();
    }

    CloseWindow();
}

void show_graph_animation(const Graph* graph, const int* path, int path_length) {
    if (graph == NULL) {
        return;
    }

    SetTraceLogLevel(LOG_NONE);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graph Simulation");
    SetTargetFPS(60);

    Vector2 positions[15];

    if (graph->num_nodes > 15) {
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
        positions[i] = get_node_position(i, graph->num_nodes);
    }

    Rectangle play_button = {30, SCREEN_HEIGHT - 80, 120, 45};

    int is_playing = 0;
    int finished = 0;

    int current_path_index = 0;
    int current_step = 0;

    float jump_timer = 0.0f;
    float wait_timer = 0.0f;

    int waiting_at_node = 0;

    if (path == NULL || path_length == 0) {
        finished = 1;
    }

    if (path_length == 1) {
        finished = 1;
    }

    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();

            if (CheckCollisionPointRec(mouse, play_button) && !finished) {
                is_playing = !is_playing;
            }
        }

        if (is_playing && !finished && path != NULL && path_length >= 2) {
            if (waiting_at_node) {
                wait_timer += delta_time;

                if (wait_timer >= NODE_WAIT_TIME) {
                    waiting_at_node = 0;
                    wait_timer = 0.0f;
                }
            } else {
                int src = path[current_path_index];
                int dst = path[current_path_index + 1];
                int edge_weight = get_edge_weight(graph, src, dst);

                jump_timer += delta_time;

                if (jump_timer >= JUMP_TIME) {
                    jump_timer = 0.0f;
                    current_step++;

                    if (current_step >= edge_weight) {
                        current_step = 0;
                        current_path_index++;

                        if (current_path_index >= path_length - 1) {
                            finished = 1;
                            is_playing = 0;
                        } else {
                            waiting_at_node = 1;
                        }
                    }
                }
            }
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Graph Simulation - Milestone 3", 30, 25, 28, BLACK);
        draw_path_text(path, path_length);

        draw_edges(graph, positions);
        draw_path_highlight(graph, path, path_length, positions);
        draw_nodes(graph, positions);

        if (path != NULL && path_length > 0) {
            Vector2 entity_position;

            if (path_length == 1 || finished) {
                entity_position = positions[path[path_length - 1]];
            } else if (waiting_at_node) {
                entity_position = positions[path[current_path_index]];
            } else {
                int src = path[current_path_index];
                int dst = path[current_path_index + 1];
                int edge_weight = get_edge_weight(graph, src, dst);

                float progress_inside_jump = jump_timer / JUMP_TIME;

                if (progress_inside_jump > 1.0f) {
                    progress_inside_jump = 1.0f;
                }

                float t = ((float)current_step + progress_inside_jump) / (float)edge_weight;

                if (t > 1.0f) {
                    t = 1.0f;
                }

                entity_position = vector_lerp(positions[src], positions[dst], t);
            }

            draw_heart(entity_position, 21.0f);
        }

        draw_button(play_button, is_playing, finished);

        if (waiting_at_node) {
            DrawText("Waiting 1 second at node...", 180, SCREEN_HEIGHT - 70, 22, DARKBLUE);
        }

        if (finished && path != NULL && path_length > 0) {
            DrawText("Arrived at destination!", 180, SCREEN_HEIGHT - 70, 24, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();
}