#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E  2.71828182845904523536
#endif

/* -------------------------------------------------------------
 * Control IDs
 * ------------------------------------------------------------- */
#define ID_EDIT_FUNC        1001
#define ID_EDIT_TITLE       1002
#define ID_EDIT_XLABEL      1003
#define ID_EDIT_YLABEL      1004
#define ID_BTN_PLOT         1005
#define ID_BTN_ZOOM_X_IN    1006
#define ID_BTN_ZOOM_X_OUT   1007
#define ID_BTN_ZOOM_Y_IN    1008
#define ID_BTN_ZOOM_Y_OUT   1009
#define ID_BTN_ZOOM_BOTH_IN 1010
#define ID_BTN_ZOOM_BOTH_OUT 1011
#define ID_BTN_RESET        1012

/* -------------------------------------------------------------
 * Math Expression Abstract Syntax Tree & Parser
 * ------------------------------------------------------------- */
typedef enum {
    NODE_NUM,
    NODE_VAR,
    NODE_UNARY_MINUS,
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_POW,
    NODE_FUNC
} NodeType;

typedef enum {
    FN_NONE,
    FN_SIN, FN_COS, FN_TAN,
    FN_ASIN, FN_ACOS, FN_ATAN,
    FN_SINH, FN_COSH, FN_TANH,
    FN_SQRT, FN_CBRT, FN_EXP,
    FN_LN, FN_LOG10, FN_ABS,
    FN_FLOOR, FN_CEIL
} FuncType;

typedef struct ExprNode {
    NodeType type;
    double val;
    FuncType func;
    struct ExprNode* left;
    struct ExprNode* right;
} ExprNode;

static ExprNode* create_node(NodeType type) {
    ExprNode* n = (ExprNode*)calloc(1, sizeof(ExprNode));
    if (n) n->type = type;
    return n;
}

static void free_node(ExprNode* n) {
    if (!n) return;
    free_node(n->left);
    free_node(n->right);
    free(n);
}

static double eval_ast(const ExprNode* n, double x) {
    if (!n) return NAN;
    switch (n->type) {
        case NODE_NUM: return n->val;
        case NODE_VAR: return x;
        case NODE_UNARY_MINUS: return -eval_ast(n->left, x);
        case NODE_ADD: return eval_ast(n->left, x) + eval_ast(n->right, x);
        case NODE_SUB: return eval_ast(n->left, x) - eval_ast(n->right, x);
        case NODE_MUL: return eval_ast(n->left, x) * eval_ast(n->right, x);
        case NODE_DIV: {
            double d = eval_ast(n->right, x);
            if (fabs(d) < 1e-15) return NAN;
            return eval_ast(n->left, x) / d;
        }
        case NODE_POW: {
            double base = eval_ast(n->left, x);
            double expn = eval_ast(n->right, x);
            if (base < 0 && floor(expn) != expn) return NAN;
            return pow(base, expn);
        }
        case NODE_FUNC: {
            double arg = eval_ast(n->left, x);
            switch (n->func) {
                case FN_SIN: return sin(arg);
                case FN_COS: return cos(arg);
                case FN_TAN: {
                    double c = cos(arg);
                    if (fabs(c) < 1e-12) return NAN;
                    return tan(arg);
                }
                case FN_ASIN: return (arg >= -1.0 && arg <= 1.0) ? asin(arg) : NAN;
                case FN_ACOS: return (arg >= -1.0 && arg <= 1.0) ? acos(arg) : NAN;
                case FN_ATAN: return atan(arg);
                case FN_SINH: return sinh(arg);
                case FN_COSH: return cosh(arg);
                case FN_TANH: return tanh(arg);
                case FN_SQRT: return arg >= 0 ? sqrt(arg) : NAN;
                case FN_CBRT: return cbrt(arg);
                case FN_EXP: return exp(arg);
                case FN_LN: return arg > 0 ? log(arg) : NAN;
                case FN_LOG10: return arg > 0 ? log10(arg) : NAN;
                case FN_ABS: return fabs(arg);
                case FN_FLOOR: return floor(arg);
                case FN_CEIL: return ceil(arg);
                default: return NAN;
            }
        }
    }
    return NAN;
}

/* Tokenizer */
typedef enum {
    TOK_EOF,
    TOK_NUM,
    TOK_VAR,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,
    TOK_POW,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_FUNC,
    TOK_ERROR
} ExprTokType;

typedef struct {
    const char* src;
    size_t pos;
    ExprTokType cur_type;
    double cur_num;
    FuncType cur_func;
    char error_msg[128];
} Parser;

static void skip_ws(Parser* p) {
    while (p->src[p->pos] && isspace((unsigned char)p->src[p->pos])) {
        p->pos++;
    }
}

static void next_token(Parser* p) {
    skip_ws(p);
    char c = p->src[p->pos];
    if (c == '\0') {
        p->cur_type = TOK_EOF;
        return;
    }

    if (isdigit((unsigned char)c) || c == '.') {
        char* endptr = NULL;
        p->cur_num = strtod(&p->src[p->pos], &endptr);
        if (endptr > &p->src[p->pos]) {
            p->pos += (size_t)(endptr - &p->src[p->pos]);
            p->cur_type = TOK_NUM;
            return;
        }
    }

    if (isalpha((unsigned char)c)) {
        size_t start = p->pos;
        while (isalnum((unsigned char)p->src[p->pos]) || p->src[p->pos] == '_') {
            p->pos++;
        }
        size_t len = p->pos - start;
        char name[32];
        if (len >= sizeof(name)) len = sizeof(name) - 1;
        memcpy(name, &p->src[start], len);
        name[len] = '\0';

        /* Case insensitive comparison */
        for (size_t i = 0; i < len; i++) name[i] = (char)tolower((unsigned char)name[i]);

        if (strcmp(name, "x") == 0) {
            p->cur_type = TOK_VAR;
            return;
        }
        if (strcmp(name, "pi") == 0) {
            p->cur_type = TOK_NUM;
            p->cur_num = M_PI;
            return;
        }
        if (strcmp(name, "e") == 0) {
            p->cur_type = TOK_NUM;
            p->cur_num = M_E;
            return;
        }

        FuncType fn = FN_NONE;
        if (strcmp(name, "sin") == 0) fn = FN_SIN;
        else if (strcmp(name, "cos") == 0) fn = FN_COS;
        else if (strcmp(name, "tan") == 0) fn = FN_TAN;
        else if (strcmp(name, "asin") == 0) fn = FN_ASIN;
        else if (strcmp(name, "acos") == 0) fn = FN_ACOS;
        else if (strcmp(name, "atan") == 0) fn = FN_ATAN;
        else if (strcmp(name, "sinh") == 0) fn = FN_SINH;
        else if (strcmp(name, "cosh") == 0) fn = FN_COSH;
        else if (strcmp(name, "tanh") == 0) fn = FN_TANH;
        else if (strcmp(name, "sqrt") == 0) fn = FN_SQRT;
        else if (strcmp(name, "cbrt") == 0) fn = FN_CBRT;
        else if (strcmp(name, "exp") == 0) fn = FN_EXP;
        else if (strcmp(name, "ln") == 0 || strcmp(name, "log") == 0) fn = FN_LN;
        else if (strcmp(name, "log10") == 0) fn = FN_LOG10;
        else if (strcmp(name, "abs") == 0 || strcmp(name, "fabs") == 0) fn = FN_ABS;
        else if (strcmp(name, "floor") == 0) fn = FN_FLOOR;
        else if (strcmp(name, "ceil") == 0) fn = FN_CEIL;

        if (fn != FN_NONE) {
            p->cur_type = TOK_FUNC;
            p->cur_func = fn;
            return;
        }

        snprintf(p->error_msg, sizeof(p->error_msg), "Unknown identifier '%s'", name);
        p->cur_type = TOK_ERROR;
        return;
    }

    p->pos++;
    switch (c) {
        case '+': p->cur_type = TOK_PLUS; break;
        case '-': p->cur_type = TOK_MINUS; break;
        case '*': p->cur_type = TOK_MUL; break;
        case '/': p->cur_type = TOK_DIV; break;
        case '^': p->cur_type = TOK_POW; break;
        case '(': p->cur_type = TOK_LPAREN; break;
        case ')': p->cur_type = TOK_RPAREN; break;
        default:
            snprintf(p->error_msg, sizeof(p->error_msg), "Unexpected character '%c'", c);
            p->cur_type = TOK_ERROR;
            break;
    }
}

/* Recursive Descent Parser Declarations */
static ExprNode* parse_expr(Parser* p);
static ExprNode* parse_term(Parser* p);
static ExprNode* parse_unary(Parser* p);
static ExprNode* parse_power(Parser* p);
static ExprNode* parse_primary(Parser* p);

static ExprNode* parse_primary(Parser* p) {
    if (p->cur_type == TOK_NUM) {
        ExprNode* n = create_node(NODE_NUM);
        n->val = p->cur_num;
        next_token(p);
        return n;
    }
    if (p->cur_type == TOK_VAR) {
        ExprNode* n = create_node(NODE_VAR);
        next_token(p);
        return n;
    }
    if (p->cur_type == TOK_FUNC) {
        FuncType fn = p->cur_func;
        next_token(p);
        if (p->cur_type != TOK_LPAREN) {
            snprintf(p->error_msg, sizeof(p->error_msg), "Expected '(' after function");
            p->cur_type = TOK_ERROR;
            return NULL;
        }
        next_token(p);
        ExprNode* arg = parse_expr(p);
        if (!arg) return NULL;
        if (p->cur_type != TOK_RPAREN) {
            snprintf(p->error_msg, sizeof(p->error_msg), "Expected ')' after function argument");
            free_node(arg);
            p->cur_type = TOK_ERROR;
            return NULL;
        }
        next_token(p);
        ExprNode* n = create_node(NODE_FUNC);
        n->func = fn;
        n->left = arg;
        return n;
    }
    if (p->cur_type == TOK_LPAREN) {
        next_token(p);
        ExprNode* inside = parse_expr(p);
        if (!inside) return NULL;
        if (p->cur_type != TOK_RPAREN) {
            snprintf(p->error_msg, sizeof(p->error_msg), "Mismatched parentheses, expected ')'");
            free_node(inside);
            p->cur_type = TOK_ERROR;
            return NULL;
        }
        next_token(p);
        return inside;
    }

    if (p->cur_type != TOK_ERROR) {
        snprintf(p->error_msg, sizeof(p->error_msg), "Unexpected token in expression");
        p->cur_type = TOK_ERROR;
    }
    return NULL;
}

static ExprNode* parse_power(Parser* p) {
    ExprNode* left = parse_primary(p);
    if (!left) return NULL;

    if (p->cur_type == TOK_POW) {
        next_token(p);
        ExprNode* right = parse_unary(p); // Right-associative power
        if (!right) {
            free_node(left);
            return NULL;
        }
        ExprNode* n = create_node(NODE_POW);
        n->left = left;
        n->right = right;
        return n;
    }
    return left;
}

static ExprNode* parse_unary(Parser* p) {
    if (p->cur_type == TOK_PLUS) {
        next_token(p);
        return parse_unary(p);
    }
    if (p->cur_type == TOK_MINUS) {
        next_token(p);
        ExprNode* sub = parse_unary(p);
        if (!sub) return NULL;
        ExprNode* n = create_node(NODE_UNARY_MINUS);
        n->left = sub;
        return n;
    }
    return parse_power(p);
}

static bool can_start_primary(ExprTokType t) {
    return (t == TOK_NUM || t == TOK_VAR || t == TOK_FUNC || t == TOK_LPAREN);
}

static ExprNode* parse_term(Parser* p) {
    ExprNode* left = parse_unary(p);
    if (!left) return NULL;

    while (p->cur_type == TOK_MUL || p->cur_type == TOK_DIV || can_start_primary(p->cur_type)) {
        if (p->cur_type == TOK_MUL) {
            next_token(p);
            ExprNode* right = parse_unary(p);
            if (!right) { free_node(left); return NULL; }
            ExprNode* n = create_node(NODE_MUL);
            n->left = left; n->right = right;
            left = n;
        } else if (p->cur_type == TOK_DIV) {
            next_token(p);
            ExprNode* right = parse_unary(p);
            if (!right) { free_node(left); return NULL; }
            ExprNode* n = create_node(NODE_DIV);
            n->left = left; n->right = right;
            left = n;
        } else if (can_start_primary(p->cur_type)) {
            /* Implicit multiplication like 2x, x(x+1), sin(x)cos(x) */
            ExprNode* right = parse_unary(p);
            if (!right) { free_node(left); return NULL; }
            ExprNode* n = create_node(NODE_MUL);
            n->left = left; n->right = right;
            left = n;
        }
    }
    return left;
}

static ExprNode* parse_expr(Parser* p) {
    ExprNode* left = parse_term(p);
    if (!left) return NULL;

    while (p->cur_type == TOK_PLUS || p->cur_type == TOK_MINUS) {
        ExprTokType op = p->cur_type;
        next_token(p);
        ExprNode* right = parse_term(p);
        if (!right) { free_node(left); return NULL; }
        ExprNode* n = create_node(op == TOK_PLUS ? NODE_ADD : NODE_SUB);
        n->left = left; n->right = right;
        left = n;
    }
    return left;
}

static ExprNode* parse_function_string(const char* str, char* out_error, size_t err_size) {
    Parser p;
    memset(&p, 0, sizeof(p));
    p.src = str;
    p.pos = 0;
    next_token(&p);

    if (p.cur_type == TOK_EOF) {
        snprintf(out_error, err_size, "Empty expression");
        return NULL;
    }

    ExprNode* root = parse_expr(&p);
    if (!root) {
        snprintf(out_error, err_size, "%s", p.error_msg[0] ? p.error_msg : "Syntax error");
        return NULL;
    }

    if (p.cur_type != TOK_EOF) {
        free_node(root);
        snprintf(out_error, err_size, "Unexpected token at end of expression");
        return NULL;
    }

    out_error[0] = '\0';
    return root;
}

/* -------------------------------------------------------------
 * Application State
 * ------------------------------------------------------------- */
typedef struct {
    /* Viewport math ranges */
    double x_min, x_max;
    double y_min, y_max;

    /* Current expression */
    char func_str[256];
    ExprNode* ast;
    char error_str[128];

    /* Metadata */
    char title_str[128];
    char xlabel_str[128];
    char ylabel_str[128];

    /* Interaction State */
    bool is_dragging;
    POINT last_mouse_pt;
    POINT cur_mouse_pt;
    bool mouse_in_graph;

    /* UI Controls */
    HWND hEditFunc;
    HWND hEditTitle;
    HWND hEditXLabel;
    HWND hEditYLabel;
    HWND hBtnPlot;
    HWND hBtnZoomXIn, hBtnZoomXOut;
    HWND hBtnZoomYIn, hBtnZoomYOut;
    HWND hBtnZoomBothIn, hBtnZoomBothOut;
    HWND hBtnReset;

    /* Fonts */
    HFONT hFontTitle;
    HFONT hFontLabel;
    HFONT hFontYAxis;
    HFONT hFontTick;
    HFONT hFontUI;
} AppState;

static AppState g_app;
static WNDPROC g_oldEditProc = NULL;

/* Subclass edit controls so pressing ENTER immediately triggers Plot */
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND hParent = GetParent(hwnd);
        PostMessage(hParent, WM_COMMAND, MAKEWPARAM(ID_BTN_PLOT, BN_CLICKED), (LPARAM)GetDlgItem(hParent, ID_BTN_PLOT));
        return 0;
    }
    return CallWindowProc(g_oldEditProc, hwnd, msg, wParam, lParam);
}

/* -------------------------------------------------------------
 * Zoom and Pan Math
 * ------------------------------------------------------------- */
static void zoom_viewport(double factor_x, double factor_y, double center_x, double center_y) {
    if (factor_x > 0.0) {
        g_app.x_min = center_x + (g_app.x_min - center_x) * factor_x;
        g_app.x_max = center_x + (g_app.x_max - center_x) * factor_x;
    }
    if (factor_y > 0.0) {
        g_app.y_min = center_y + (g_app.y_min - center_y) * factor_y;
        g_app.y_max = center_y + (g_app.y_max - center_y) * factor_y;
    }

    /* Prevent degenerate view */
    if (fabs(g_app.x_max - g_app.x_min) < 1e-12) {
        g_app.x_min = center_x - 1e-12;
        g_app.x_max = center_x + 1e-12;
    }
    if (fabs(g_app.y_max - g_app.y_min) < 1e-12) {
        g_app.y_min = center_y - 1e-12;
        g_app.y_max = center_y + 1e-12;
    }
}

static void reset_viewport(void) {
    g_app.x_min = -10.0;
    g_app.x_max =  10.0;
    g_app.y_min = -10.0;
    g_app.y_max =  10.0;
}

/* -------------------------------------------------------------
 * Coordinate Conversions & Tick Steps
 * ------------------------------------------------------------- */
static inline int to_screen_x(double x, RECT gr) {
    return gr.left + (int)((x - g_app.x_min) / (g_app.x_max - g_app.x_min) * (gr.right - gr.left));
}

static inline int to_screen_y(double y, RECT gr) {
    return gr.bottom - (int)((y - g_app.y_min) / (g_app.y_max - g_app.y_min) * (gr.bottom - gr.top));
}

static inline double to_math_x(int sx, RECT gr) {
    return g_app.x_min + (double)(sx - gr.left) / (double)(gr.right - gr.left) * (g_app.x_max - g_app.x_min);
}

static inline double to_math_y(int sy, RECT gr) {
    return g_app.y_min + (double)(gr.bottom - sy) / (double)(gr.bottom - gr.top) * (g_app.y_max - g_app.y_min);
}

static double calculate_nice_step(double span, int max_ticks) {
    if (span <= 0 || max_ticks <= 0) return 1.0;
    double raw_step = span / max_ticks;
    double exponent = floor(log10(raw_step));
    double fraction = raw_step / pow(10.0, exponent);
    double nice_fraction;

    if (fraction < 1.5) nice_fraction = 1.0;
    else if (fraction < 3.0) nice_fraction = 2.0;
    else if (fraction < 7.0) nice_fraction = 5.0;
    else nice_fraction = 10.0;

    return nice_fraction * pow(10.0, exponent);
}

static void format_tick_label(char* buf, size_t sz, double val, double step) {
    if (fabs(val) < 1e-12) {
        snprintf(buf, sz, "0");
        return;
    }
    double mag = fabs(step);
    if (mag >= 1.0 && mag < 10000.0) {
        snprintf(buf, sz, "%.0f", val);
    } else if (mag >= 0.01 && mag < 1.0) {
        snprintf(buf, sz, "%.2f", val);
    } else if (mag >= 0.0001 && mag < 0.01) {
        snprintf(buf, sz, "%.4f", val);
    } else {
        snprintf(buf, sz, "%.2e", val);
    }
}

/* -------------------------------------------------------------
 * Graph Rendering
 * ------------------------------------------------------------- */
static void draw_graph(HDC hdc, RECT rcClient) {
    /* Define layout */
    int top_bar_height = 82;
    int bottom_bar_height = 36;
    
    RECT rcGraph;
    rcGraph.left   = 85;
    rcGraph.top    = top_bar_height + 40;  /* Space for title */
    rcGraph.right  = rcClient.right - 35;
    rcGraph.bottom = rcClient.bottom - bottom_bar_height - 35; /* Space for X-label */

    if (rcGraph.right <= rcGraph.left + 50 || rcGraph.bottom <= rcGraph.top + 50) {
        return;
    }

    /* 1. Fill Backgrounds */
    HBRUSH hBrToolbar = CreateSolidBrush(RGB(245, 247, 250));
    RECT rcTopBar = {0, 0, rcClient.right, top_bar_height};
    FillRect(hdc, &rcTopBar, hBrToolbar);
    DeleteObject(hBrToolbar);

    /* Separator line below toolbar */
    HPEN hPenSep = CreatePen(PS_SOLID, 1, RGB(218, 222, 229));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPenSep);
    MoveToEx(hdc, 0, top_bar_height, NULL);
    LineTo(hdc, rcClient.right, top_bar_height);

    /* Graph Canvas Background */
    RECT rcCanvas = {0, top_bar_height + 1, rcClient.right, rcClient.bottom};
    HBRUSH hBrCanvas = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rcCanvas, hBrCanvas);
    DeleteObject(hBrCanvas);

    /* Graph Plot Area Background */
    HBRUSH hBrPlot = CreateSolidBrush(RGB(252, 253, 255));
    FillRect(hdc, &rcGraph, hBrPlot);
    DeleteObject(hBrPlot);

    /* Graph Border */
    HPEN hPenBorder = CreatePen(PS_SOLID, 1, RGB(200, 208, 218));
    SelectObject(hdc, hPenBorder);
    Rectangle(hdc, rcGraph.left, rcGraph.top, rcGraph.right, rcGraph.bottom);
    DeleteObject(hPenBorder);

    /* 2. Title */
    HFONT hOldFont = (HFONT)SelectObject(hdc, g_app.hFontTitle);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(25, 35, 55));
    RECT rcTitle = {rcGraph.left, top_bar_height + 8, rcGraph.right, rcGraph.top};
    DrawTextA(hdc, g_app.title_str, -1, &rcTitle, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    /* 3. Grid Lines & Ticks */
    HPEN hPenGrid = CreatePen(PS_SOLID, 1, RGB(232, 237, 243));
    HPEN hPenAxis = CreatePen(PS_SOLID, 2, RGB(70, 80, 95));
    SelectObject(hdc, g_app.hFontTick);
    SetTextColor(hdc, RGB(100, 110, 125));

    double x_span = g_app.x_max - g_app.x_min;
    double y_span = g_app.y_max - g_app.y_min;
    double x_step = calculate_nice_step(x_span, 10);
    double y_step = calculate_nice_step(y_span, 8);

    /* Vertical Grid lines & X-Ticks */
    double first_x = ceil(g_app.x_min / x_step) * x_step;
    for (double x = first_x; x <= g_app.x_max + 1e-9; x += x_step) {
        int sx = to_screen_x(x, rcGraph);
        if (sx >= rcGraph.left && sx <= rcGraph.right) {
            SelectObject(hdc, hPenGrid);
            MoveToEx(hdc, sx, rcGraph.top, NULL);
            LineTo(hdc, sx, rcGraph.bottom);

            /* Tick mark */
            SelectObject(hdc, hPenAxis);
            MoveToEx(hdc, sx, rcGraph.bottom, NULL);
            LineTo(hdc, sx, rcGraph.bottom + 5);

            /* Tick label */
            char lbl[32];
            format_tick_label(lbl, sizeof(lbl), x, x_step);
            RECT rcLabel = {sx - 40, rcGraph.bottom + 7, sx + 40, rcGraph.bottom + 24};
            DrawTextA(hdc, lbl, -1, &rcLabel, DT_CENTER | DT_SINGLELINE);
        }
    }

    /* Horizontal Grid lines & Y-Ticks */
    double first_y = ceil(g_app.y_min / y_step) * y_step;
    for (double y = first_y; y <= g_app.y_max + 1e-9; y += y_step) {
        int sy = to_screen_y(y, rcGraph);
        if (sy >= rcGraph.top && sy <= rcGraph.bottom) {
            SelectObject(hdc, hPenGrid);
            MoveToEx(hdc, rcGraph.left, sy, NULL);
            LineTo(hdc, rcGraph.right, sy);

            /* Tick mark */
            SelectObject(hdc, hPenAxis);
            MoveToEx(hdc, rcGraph.left - 5, sy, NULL);
            LineTo(hdc, rcGraph.left, sy);

            /* Tick label */
            char lbl[32];
            format_tick_label(lbl, sizeof(lbl), y, y_step);
            RECT rcLabel = {rcGraph.left - 80, sy - 8, rcGraph.left - 8, sy + 8};
            DrawTextA(hdc, lbl, -1, &rcLabel, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }
    }

    /* Major Axes (X = 0 and Y = 0) */
    SelectObject(hdc, hPenAxis);
    int origin_x = to_screen_x(0.0, rcGraph);
    int origin_y = to_screen_y(0.0, rcGraph);

    if (origin_x >= rcGraph.left && origin_x <= rcGraph.right) {
        MoveToEx(hdc, origin_x, rcGraph.top, NULL);
        LineTo(hdc, origin_x, rcGraph.bottom);
    }
    if (origin_y >= rcGraph.top && origin_y <= rcGraph.bottom) {
        MoveToEx(hdc, rcGraph.left, origin_y, NULL);
        LineTo(hdc, rcGraph.right, origin_y);
    }

    /* 4. Function Plotting with Clipping */
    HRGN hRgnClip = CreateRectRgn(rcGraph.left, rcGraph.top, rcGraph.right, rcGraph.bottom);
    SelectClipRgn(hdc, hRgnClip);

    if (g_app.ast) {
        HPEN hPenCurve = CreatePen(PS_SOLID, 2, RGB(28, 100, 242)); // Vibrant blue
        SelectObject(hdc, hPenCurve);

        bool has_prev = false;
        int prev_sy = 0;
        double prev_y = 0.0;

        int total_pixels = rcGraph.right - rcGraph.left;
        for (int i = 0; i <= total_pixels; i++) {
            int sx = rcGraph.left + i;
            double mx = to_math_x(sx, rcGraph);
            double my = eval_ast(g_app.ast, mx);

            if (!isnan(my) && !isinf(my)) {
                int sy = to_screen_y(my, rcGraph);

                /* Detect asymptote / jump discontinuity (e.g. tan(x), 1/x) */
                bool jump = false;
                if (has_prev) {
                    if (abs(sy - prev_sy) > (rcGraph.bottom - rcGraph.top) * 2) {
                        if ((prev_y > 0 && my < 0) || (prev_y < 0 && my > 0)) {
                            jump = true;
                        }
                    }
                }

                if (has_prev && !jump) {
                    LineTo(hdc, sx, sy);
                } else {
                    MoveToEx(hdc, sx, sy, NULL);
                }
                has_prev = true;
                prev_sy = sy;
                prev_y = my;
            } else {
                has_prev = false;
            }
        }
        DeleteObject(hPenCurve);
    }

    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgnClip);

    /* 5. Axis Labels */
    SelectObject(hdc, g_app.hFontLabel);
    SetTextColor(hdc, RGB(40, 50, 70));

    /* X Axis Label (bottom center) */
    RECT rcXLabel = {rcGraph.left, rcGraph.bottom + 25, rcGraph.right, rcClient.bottom - bottom_bar_height};
    DrawTextA(hdc, g_app.xlabel_str, -1, &rcXLabel, DT_CENTER | DT_SINGLELINE);

    /* Y Axis Label (vertical on the left) */
    SelectObject(hdc, g_app.hFontYAxis);
    int y_center = (rcGraph.top + rcGraph.bottom) / 2;
    SIZE sz;
    GetTextExtentPoint32A(hdc, g_app.ylabel_str, (int)strlen(g_app.ylabel_str), &sz);
    TextOutA(hdc, 20, y_center + sz.cx / 2, g_app.ylabel_str, (int)strlen(g_app.ylabel_str));

    /* 6. Error or Status Message */
    SelectObject(hdc, g_app.hFontUI);
    if (g_app.error_str[0]) {
        SetTextColor(hdc, RGB(220, 38, 38)); // Red
        char err_disp[200];
        snprintf(err_disp, sizeof(err_disp), "Expression Error: %s", g_app.error_str);
        RECT rcErr = {rcGraph.left + 10, rcGraph.top + 10, rcGraph.right - 10, rcGraph.top + 35};
        DrawTextA(hdc, err_disp, -1, &rcErr, DT_LEFT | DT_SINGLELINE);
    }

    /* 7. Bottom Navigation & Coordinate Bar */
    RECT rcBottomBar = {0, rcClient.bottom - bottom_bar_height, rcClient.right, rcClient.bottom};
    HBRUSH hBrBottom = CreateSolidBrush(RGB(248, 250, 252));
    FillRect(hdc, &rcBottomBar, hBrBottom);
    DeleteObject(hBrBottom);

    SelectObject(hdc, hPenSep);
    MoveToEx(hdc, 0, rcBottomBar.top, NULL);
    LineTo(hdc, rcClient.right, rcBottomBar.top);

    SetTextColor(hdc, RGB(110, 120, 135));
    RECT rcHelp = {15, rcBottomBar.top, rcClient.right - 300, rcBottomBar.bottom};
    DrawTextA(hdc, "Tips: Drag to Pan  |  Scroll: Zoom Both  |  Shift+Scroll: Zoom X  |  Ctrl+Scroll: Zoom Y", 
              -1, &rcHelp, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    /* Mouse Coordinate Readout */
    if (g_app.mouse_in_graph) {
        double mx = to_math_x(g_app.cur_mouse_pt.x, rcGraph);
        double my = to_math_y(g_app.cur_mouse_pt.y, rcGraph);
        char coord_buf[128];
        snprintf(coord_buf, sizeof(coord_buf), "X: %+.4f   Y: %+.4f", mx, my);
        SetTextColor(hdc, RGB(30, 41, 59));
        RECT rcCoord = {rcClient.right - 280, rcBottomBar.top, rcClient.right - 15, rcBottomBar.bottom};
        DrawTextA(hdc, coord_buf, -1, &rcCoord, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    } else {
        char range_buf[128];
        snprintf(range_buf, sizeof(range_buf), "X: [%.2f, %.2f]  Y: [%.2f, %.2f]", g_app.x_min, g_app.x_max, g_app.y_min, g_app.y_max);
        SetTextColor(hdc, RGB(100, 116, 139));
        RECT rcCoord = {rcClient.right - 320, rcBottomBar.top, rcClient.right - 15, rcBottomBar.bottom};
        DrawTextA(hdc, range_buf, -1, &rcCoord, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }

    /* Cleanup DC selections */
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldFont);
    DeleteObject(hPenSep);
    DeleteObject(hPenGrid);
    DeleteObject(hPenAxis);
}

/* -------------------------------------------------------------
 * UI Creation & Management
 * ------------------------------------------------------------- */
static void create_ui_controls(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

    g_app.hFontUI    = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_app.hFontTitle = CreateFontA(20, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_app.hFontLabel = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_app.hFontYAxis = CreateFontA(15, 0, 900, 900, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_app.hFontTick  = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    /* Row 1: Function, Title, Axis Labels, Plot Button */
    CreateWindowA("STATIC", "f(x) =", WS_CHILD | WS_VISIBLE, 12, 12, 38, 22, hwnd, NULL, hInst, NULL);
    g_app.hEditFunc = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_app.func_str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 52, 10, 190, 24, hwnd, (HMENU)ID_EDIT_FUNC, hInst, NULL);

    CreateWindowA("STATIC", "Title:", WS_CHILD | WS_VISIBLE, 252, 12, 38, 22, hwnd, NULL, hInst, NULL);
    g_app.hEditTitle = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_app.title_str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 292, 10, 150, 24, hwnd, (HMENU)ID_EDIT_TITLE, hInst, NULL);

    CreateWindowA("STATIC", "X-Axis:", WS_CHILD | WS_VISIBLE, 452, 12, 45, 22, hwnd, NULL, hInst, NULL);
    g_app.hEditXLabel = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_app.xlabel_str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 500, 10, 90, 24, hwnd, (HMENU)ID_EDIT_XLABEL, hInst, NULL);

    CreateWindowA("STATIC", "Y-Axis:", WS_CHILD | WS_VISIBLE, 600, 12, 45, 22, hwnd, NULL, hInst, NULL);
    g_app.hEditYLabel = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_app.ylabel_str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 648, 10, 90, 24, hwnd, (HMENU)ID_EDIT_YLABEL, hInst, NULL);

    g_app.hBtnPlot = CreateWindowA("BUTTON", "Plot / Update", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 750, 9, 105, 26, hwnd, (HMENU)ID_BTN_PLOT, hInst, NULL);

    /* Row 2: Zoom Controls */
    CreateWindowA("STATIC", "Zoom:", WS_CHILD | WS_VISIBLE, 12, 48, 42, 22, hwnd, NULL, hInst, NULL);
    g_app.hBtnZoomXIn     = CreateWindowA("BUTTON", "X +", WS_CHILD | WS_VISIBLE, 56, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_X_IN, hInst, NULL);
    g_app.hBtnZoomXOut    = CreateWindowA("BUTTON", "X -", WS_CHILD | WS_VISIBLE, 108, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_X_OUT, hInst, NULL);

    g_app.hBtnZoomYIn     = CreateWindowA("BUTTON", "Y +", WS_CHILD | WS_VISIBLE, 168, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_Y_IN, hInst, NULL);
    g_app.hBtnZoomYOut    = CreateWindowA("BUTTON", "Y -", WS_CHILD | WS_VISIBLE, 220, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_Y_OUT, hInst, NULL);

    g_app.hBtnZoomBothIn  = CreateWindowA("BUTTON", "Both +", WS_CHILD | WS_VISIBLE, 280, 45, 62, 25, hwnd, (HMENU)ID_BTN_ZOOM_BOTH_IN, hInst, NULL);
    g_app.hBtnZoomBothOut = CreateWindowA("BUTTON", "Both -", WS_CHILD | WS_VISIBLE, 346, 45, 62, 25, hwnd, (HMENU)ID_BTN_ZOOM_BOTH_OUT, hInst, NULL);

    g_app.hBtnReset       = CreateWindowA("BUTTON", "Reset View", WS_CHILD | WS_VISIBLE, 420, 45, 85, 25, hwnd, (HMENU)ID_BTN_RESET, hInst, NULL);

    /* Subclass edit controls so pressing Enter automatically plots */
    g_oldEditProc = (WNDPROC)SetWindowLongPtr(g_app.hEditFunc, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    SetWindowLongPtr(g_app.hEditTitle,  GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    SetWindowLongPtr(g_app.hEditXLabel, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    SetWindowLongPtr(g_app.hEditYLabel, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    /* Set clean font for controls */
    HWND controls[] = {
        g_app.hEditFunc, g_app.hEditTitle, g_app.hEditXLabel, g_app.hEditYLabel,
        g_app.hBtnPlot, g_app.hBtnZoomXIn, g_app.hBtnZoomXOut, g_app.hBtnZoomYIn,
        g_app.hBtnZoomYOut, g_app.hBtnZoomBothIn, g_app.hBtnZoomBothOut, g_app.hBtnReset
    };
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); i++) {
        SendMessage(controls[i], WM_SETFONT, (WPARAM)g_app.hFontUI, TRUE);
    }
}

static void update_from_ui(HWND hwnd) {
    GetWindowTextA(g_app.hEditFunc, g_app.func_str, sizeof(g_app.func_str));
    GetWindowTextA(g_app.hEditTitle, g_app.title_str, sizeof(g_app.title_str));
    GetWindowTextA(g_app.hEditXLabel, g_app.xlabel_str, sizeof(g_app.xlabel_str));
    GetWindowTextA(g_app.hEditYLabel, g_app.ylabel_str, sizeof(g_app.ylabel_str));

    if (g_app.ast) {
        free_node(g_app.ast);
        g_app.ast = NULL;
    }

    g_app.ast = parse_function_string(g_app.func_str, g_app.error_str, sizeof(g_app.error_str));
    InvalidateRect(hwnd, NULL, FALSE);
}

/* -------------------------------------------------------------
 * Main Window Procedure
 * ------------------------------------------------------------- */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            create_ui_controls(hwnd);
            update_from_ui(hwnd);
            return 0;

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case ID_BTN_PLOT:
                    update_from_ui(hwnd);
                    break;
                case ID_BTN_ZOOM_X_IN:
                    zoom_viewport(0.8, 1.0, (g_app.x_min + g_app.x_max) / 2.0, (g_app.y_min + g_app.y_max) / 2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_BTN_ZOOM_X_OUT:
                    zoom_viewport(1.25, 1.0, (g_app.x_min + g_app.x_max) / 2.0, (g_app.y_min + g_app.y_max) / 2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_BTN_ZOOM_Y_IN:
                    zoom_viewport(1.0, 0.8, (g_app.x_min + g_app.x_max) / 2.0, (g_app.y_min + g_app.y_max) / 2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_BTN_ZOOM_Y_OUT:
                    zoom_viewport(1.0, 1.25, (g_app.x_min + g_app.x_max) / 2.0, (g_app.y_min + g_app.y_max) / 2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_BTN_ZOOM_BOTH_IN:
                    zoom_viewport(0.8, 0.8, (g_app.x_min + g_app.x_max) / 2.0, (g_app.y_min + g_app.y_max) / 2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_BTN_ZOOM_BOTH_OUT:
                    zoom_viewport(1.25, 1.25, (g_app.x_min + g_app.x_max) / 2.0, (g_app.y_min + g_app.y_max) / 2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_BTN_RESET:
                    reset_viewport();
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            double factor = (delta > 0) ? 0.85 : (1.0 / 0.85);

            /* Check modifier keys */
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            ScreenToClient(hwnd, &pt);

            double cx = (g_app.x_min + g_app.x_max) / 2.0;
            double cy = (g_app.y_min + g_app.y_max) / 2.0;

            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            RECT gr = {85, 122, rcClient.right - 35, rcClient.bottom - 71};
            if (pt.x >= gr.left && pt.x <= gr.right && pt.y >= gr.top && pt.y <= gr.bottom) {
                cx = to_math_x(pt.x, gr);
                cy = to_math_y(pt.y, gr);
            }

            if (shift) {
                /* Zoom X-Axis only */
                zoom_viewport(factor, 1.0, cx, cy);
            } else if (ctrl) {
                /* Zoom Y-Axis only */
                zoom_viewport(1.0, factor, cx, cy);
            } else {
                /* Zoom Both */
                zoom_viewport(factor, factor, cx, cy);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            RECT gr = {85, 122, rcClient.right - 35, rcClient.bottom - 71};

            if (pt.x >= gr.left && pt.x <= gr.right && pt.y >= gr.top && pt.y <= gr.bottom) {
                g_app.is_dragging = true;
                g_app.last_mouse_pt = pt;
                SetCapture(hwnd);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            RECT gr = {85, 122, rcClient.right - 35, rcClient.bottom - 71};

            g_app.cur_mouse_pt = pt;
            g_app.mouse_in_graph = (pt.x >= gr.left && pt.x <= gr.right && pt.y >= gr.top && pt.y <= gr.bottom);

            if (g_app.is_dragging) {
                int dx = pt.x - g_app.last_mouse_pt.x;
                int dy = pt.y - g_app.last_mouse_pt.y;

                double x_span = g_app.x_max - g_app.x_min;
                double y_span = g_app.y_max - g_app.y_min;

                double math_dx = (double)dx / (double)(gr.right - gr.left) * x_span;
                double math_dy = (double)dy / (double)(gr.bottom - gr.top) * y_span;

                g_app.x_min -= math_dx;
                g_app.x_max -= math_dx;
                g_app.y_min += math_dy;
                g_app.y_max += math_dy;

                g_app.last_mouse_pt = pt;
            }

            /* Redraw to update coordinates or pan view */
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_app.is_dragging) {
                g_app.is_dragging = false;
                ReleaseCapture();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            switch (wParam) {
                case 'R':
                    reset_viewport();
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case 'X':
                    if (GetKeyState(VK_SHIFT) & 0x8000)
                        zoom_viewport(1.25, 1.0, (g_app.x_min + g_app.x_max)/2.0, (g_app.y_min + g_app.y_max)/2.0);
                    else
                        zoom_viewport(0.8, 1.0, (g_app.x_min + g_app.x_max)/2.0, (g_app.y_min + g_app.y_max)/2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case 'Y':
                    if (GetKeyState(VK_SHIFT) & 0x8000)
                        zoom_viewport(1.0, 1.25, (g_app.x_min + g_app.x_max)/2.0, (g_app.y_min + g_app.y_max)/2.0);
                    else
                        zoom_viewport(1.0, 0.8, (g_app.x_min + g_app.x_max)/2.0, (g_app.y_min + g_app.y_max)/2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case VK_OEM_PLUS:
                case VK_ADD:
                    zoom_viewport(0.8, 0.8, (g_app.x_min + g_app.x_max)/2.0, (g_app.y_min + g_app.y_max)/2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case VK_OEM_MINUS:
                case VK_SUBTRACT:
                    zoom_viewport(1.25, 1.25, (g_app.x_min + g_app.x_max)/2.0, (g_app.y_min + g_app.y_max)/2.0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case VK_LEFT: {
                    double span = g_app.x_max - g_app.x_min;
                    g_app.x_min -= span * 0.1;
                    g_app.x_max -= span * 0.1;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
                case VK_RIGHT: {
                    double span = g_app.x_max - g_app.x_min;
                    g_app.x_min += span * 0.1;
                    g_app.x_max += span * 0.1;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
                case VK_UP: {
                    double span = g_app.y_max - g_app.y_min;
                    g_app.y_min += span * 0.1;
                    g_app.y_max += span * 0.1;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
                case VK_DOWN: {
                    double span = g_app.y_max - g_app.y_min;
                    g_app.y_min -= span * 0.1;
                    g_app.y_max -= span * 0.1;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            /* Double-buffering for flicker-free display */
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

            draw_graph(memDC, rcClient);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; /* Handled in double buffer */

        case WM_DESTROY:
            if (g_app.ast) free_node(g_app.ast);
            if (g_app.hFontTitle) DeleteObject(g_app.hFontTitle);
            if (g_app.hFontLabel) DeleteObject(g_app.hFontLabel);
            if (g_app.hFontYAxis) DeleteObject(g_app.hFontYAxis);
            if (g_app.hFontTick)  DeleteObject(g_app.hFontTick);
            if (g_app.hFontUI)    DeleteObject(g_app.hFontUI);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* -------------------------------------------------------------
 * Application Entry Point
 * ------------------------------------------------------------- */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    /* Initialize default state */
    memset(&g_app, 0, sizeof(g_app));
    g_app.x_min = -10.0;
    g_app.x_max =  10.0;
    g_app.y_min = -10.0;
    g_app.y_max =  10.0;

    strncpy(g_app.func_str, "sin(x) * x / 2", sizeof(g_app.func_str) - 1);
    strncpy(g_app.title_str, "f(x) = sin(x) * x / 2", sizeof(g_app.title_str) - 1);
    strncpy(g_app.xlabel_str, "Time / Position (x)", sizeof(g_app.xlabel_str) - 1);
    strncpy(g_app.ylabel_str, "Amplitude f(x)", sizeof(g_app.ylabel_str) - 1);

    /* Register Window Class */
    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CFunctionPlotterWnd";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Error", MB_ICONERROR);
        return 1;
    }

    /* Create Main Window */
    HWND hwnd = CreateWindowExA(
        0,
        "CFunctionPlotterWnd",
        "C Interactive Function Plotter - f(x)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 680,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create main window.", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Message Loop */
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}

