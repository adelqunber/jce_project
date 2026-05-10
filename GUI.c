#include "GUI.h"
#include "raylib.h"

#include <math.h>
#include <stddef.h>

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700

#define NODE_RADIUS 25.0f
#define ARROW_SIZE 12.0f

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