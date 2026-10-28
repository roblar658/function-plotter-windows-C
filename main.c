#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E  2.71828182845904523536
#endif

/* -------------------------------------------------------------
 * Control & Menu IDs
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
#define ID_BTN_EXCEL        1013

#define ID_FILE_SAVE        2001
#define ID_FILE_SAVE_EXCEL  2002
#define ID_FILE_SAVE_PNG    2003
#define ID_FILE_EXIT        2004
#define ID_HELP_ABOUT       2005

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
        ExprNode* right = parse_unary(p);
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
            /* Implicit multiplication */
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
    HWND hBtnExcel;

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
 * Graph Rendering (Supports Window and Export Modes)
 * ------------------------------------------------------------- */
static void draw_graph(HDC hdc, RECT rcClient, bool for_export) {
    int top_bar_height = for_export ? 0 : 82;
    int bottom_bar_height = for_export ? 0 : 36;
    
    RECT rcGraph;
    rcGraph.left   = for_export ? 95 : 85;
    rcGraph.top    = for_export ? 65 : (top_bar_height + 40);
    rcGraph.right  = rcClient.right - 40;
    rcGraph.bottom = rcClient.bottom - bottom_bar_height - (for_export ? 50 : 35);

    if (rcGraph.right <= rcGraph.left + 50 || rcGraph.bottom <= rcGraph.top + 50) {
        return;
    }

    /* 1. Backgrounds */
    if (!for_export) {
        HBRUSH hBrToolbar = CreateSolidBrush(RGB(245, 247, 250));
        RECT rcTopBar = {0, 0, rcClient.right, top_bar_height};
        FillRect(hdc, &rcTopBar, hBrToolbar);
        DeleteObject(hBrToolbar);

        HPEN hPenSep = CreatePen(PS_SOLID, 1, RGB(218, 222, 229));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPenSep);
        MoveToEx(hdc, 0, top_bar_height, NULL);
        LineTo(hdc, rcClient.right, top_bar_height);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPenSep);
    }

    RECT rcCanvas = {0, top_bar_height, rcClient.right, rcClient.bottom};
    HBRUSH hBrCanvas = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rcCanvas, hBrCanvas);
    DeleteObject(hBrCanvas);

    HBRUSH hBrPlot = CreateSolidBrush(RGB(252, 253, 255));
    FillRect(hdc, &rcGraph, hBrPlot);
    DeleteObject(hBrPlot);

    HPEN hPenBorder = CreatePen(PS_SOLID, 1, RGB(200, 208, 218));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPenBorder);
    Rectangle(hdc, rcGraph.left, rcGraph.top, rcGraph.right, rcGraph.bottom);

    /* 2. Title */
    HFONT hOldFont = (HFONT)SelectObject(hdc, g_app.hFontTitle);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(25, 35, 55));
    RECT rcTitle = {rcGraph.left, for_export ? 15 : (top_bar_height + 8), rcGraph.right, rcGraph.top - 8};
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

            SelectObject(hdc, hPenAxis);
            MoveToEx(hdc, sx, rcGraph.bottom, NULL);
            LineTo(hdc, sx, rcGraph.bottom + 5);

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

            SelectObject(hdc, hPenAxis);
            MoveToEx(hdc, rcGraph.left - 5, sy, NULL);
            LineTo(hdc, rcGraph.left, sy);

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
        HPEN hPenCurve = CreatePen(PS_SOLID, 2, RGB(28, 100, 242));
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

    /* X Axis Label */
    RECT rcXLabel = {rcGraph.left, rcGraph.bottom + 25, rcGraph.right, rcGraph.bottom + 50};
    DrawTextA(hdc, g_app.xlabel_str, -1, &rcXLabel, DT_CENTER | DT_SINGLELINE);

    /* Y Axis Label */
    SelectObject(hdc, g_app.hFontYAxis);
    int y_center = (rcGraph.top + rcGraph.bottom) / 2;
    SIZE sz;
    GetTextExtentPoint32A(hdc, g_app.ylabel_str, (int)strlen(g_app.ylabel_str), &sz);
    TextOutA(hdc, for_export ? 25 : 20, y_center + sz.cx / 2, g_app.ylabel_str, (int)strlen(g_app.ylabel_str));

    /* 6. Status/Error on UI */
    SelectObject(hdc, g_app.hFontUI);
    if (g_app.error_str[0]) {
        SetTextColor(hdc, RGB(220, 38, 38));
        char err_disp[200];
        snprintf(err_disp, sizeof(err_disp), "Expression Error: %s", g_app.error_str);
        RECT rcErr = {rcGraph.left + 10, rcGraph.top + 10, rcGraph.right - 10, rcGraph.top + 35};
        DrawTextA(hdc, err_disp, -1, &rcErr, DT_LEFT | DT_SINGLELINE);
    }

    /* 7. Bottom Navigation & Readout */
    if (!for_export) {
        RECT rcBottomBar = {0, rcClient.bottom - bottom_bar_height, rcClient.right, rcClient.bottom};
        HBRUSH hBrBottom = CreateSolidBrush(RGB(248, 250, 252));
        FillRect(hdc, &rcBottomBar, hBrBottom);
        DeleteObject(hBrBottom);

        HPEN hPenSep = CreatePen(PS_SOLID, 1, RGB(218, 222, 229));
        SelectObject(hdc, hPenSep);
        MoveToEx(hdc, 0, rcBottomBar.top, NULL);
        LineTo(hdc, rcClient.right, rcBottomBar.top);
        DeleteObject(hPenSep);

        SetTextColor(hdc, RGB(110, 120, 135));
        RECT rcHelp = {15, rcBottomBar.top, rcClient.right - 300, rcBottomBar.bottom};
        DrawTextA(hdc, "Tips: Drag to Pan | Scroll: Zoom Both | Shift+Scroll: Zoom X | Ctrl+Scroll: Zoom Y | Ctrl+S: Excel", 
                  -1, &rcHelp, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

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
    } else {
        SetTextColor(hdc, RGB(140, 150, 165));
        SelectObject(hdc, g_app.hFontTick);
        char note[512];
        snprintf(note, sizeof(note), "f(x) = %s  |  X: [%.3f, %.3f]  Y: [%.3f, %.3f]",
                 g_app.func_str, g_app.x_min, g_app.x_max, g_app.y_min, g_app.y_max);
        RECT rcNote = {rcGraph.left, rcClient.bottom - 22, rcGraph.right, rcClient.bottom - 4};
        DrawTextA(hdc, note, -1, &rcNote, DT_RIGHT | DT_SINGLELINE);
    }

    /* Cleanup DC selections */
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldFont);
    DeleteObject(hPenBorder);
    DeleteObject(hPenGrid);
    DeleteObject(hPenAxis);
}

/* -------------------------------------------------------------
 * PNG & ZIP Generation for Excel Export
 * ------------------------------------------------------------- */
static uint32_t crc_table[256];
static int crc_ready = 0;

static void init_crc(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    crc_ready = 1;
}

static uint32_t update_crc(uint32_t crc, const uint8_t *buf, size_t len) {
    uint32_t c = crc ^ 0xFFFFFFFFL;
    if (!crc_ready) init_crc();
    for (size_t n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFL;
}

static uint32_t update_adler32(uint32_t adler, const uint8_t *buf, size_t len) {
    uint32_t s1 = adler & 0xFFFF;
    uint32_t s2 = (adler >> 16) & 0xFFFF;
    for (size_t n = 0; n < len; n++) {
        s1 = (s1 + buf[n]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    return (s2 << 16) | s1;
}

#define WRITE_U32_BE(ptr, val) do { \
    (ptr)[0] = (uint8_t)((val) >> 24); \
    (ptr)[1] = (uint8_t)((val) >> 16); \
    (ptr)[2] = (uint8_t)((val) >> 8);  \
    (ptr)[3] = (uint8_t)((val));       \
} while(0)

static uint8_t* create_png_rgb24(const uint8_t* rgb, int w, int h, size_t* out_size) {
    size_t line_len = 1 + (size_t)w * 3;
    size_t raw_size = line_len * (size_t)h;
    uint8_t* raw = (uint8_t*)malloc(raw_size);
    if (!raw) return NULL;
    for (int y = 0; y < h; y++) {
        raw[y * line_len] = 0;
        memcpy(&raw[y * line_len + 1], &rgb[y * w * 3], (size_t)w * 3);
    }

    uint32_t adler = update_adler32(1, raw, raw_size);
    size_t num_blocks = (raw_size + 65534) / 65535;
    if (num_blocks == 0) num_blocks = 1;
    size_t idat_data_size = 2 + num_blocks * 5 + raw_size + 4;
    size_t total_png = 8 + (8 + 13 + 4) + (8 + idat_data_size + 4) + (8 + 0 + 4);

    uint8_t* png = (uint8_t*)malloc(total_png);
    if (!png) { free(raw); return NULL; }

    uint8_t* p = png;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    memcpy(p, sig, 8); p += 8;

    /* IHDR */
    WRITE_U32_BE(p, 13); p += 4;
    uint8_t* ihdr_start = p;
    memcpy(p, "IHDR", 4); p += 4;
    WRITE_U32_BE(p, (uint32_t)w); p += 4;
    WRITE_U32_BE(p, (uint32_t)h); p += 4;
    *p++ = 8; *p++ = 2; *p++ = 0; *p++ = 0; *p++ = 0;
    uint32_t ihdr_crc = update_crc(0, ihdr_start, 17);
    WRITE_U32_BE(p, ihdr_crc); p += 4;

    /* IDAT */
    WRITE_U32_BE(p, (uint32_t)idat_data_size); p += 4;
    uint8_t* idat_start = p;
    memcpy(p, "IDAT", 4); p += 4;
    *p++ = 0x78; *p++ = 0x01;
    size_t offset = 0;
    while (offset < raw_size) {
        size_t chunk = raw_size - offset;
        if (chunk > 65535) chunk = 65535;
        uint8_t is_last = (offset + chunk >= raw_size) ? 1 : 0;
        *p++ = is_last;
        *p++ = (uint8_t)(chunk & 0xFF);
        *p++ = (uint8_t)((chunk >> 8) & 0xFF);
        *p++ = (uint8_t)((~chunk) & 0xFF);
        *p++ = (uint8_t)(((~chunk) >> 8) & 0xFF);
        memcpy(p, &raw[offset], chunk);
        p += chunk;
        offset += chunk;
    }
    WRITE_U32_BE(p, adler); p += 4;
    uint32_t idat_crc = update_crc(0, idat_start, 4 + idat_data_size);
    WRITE_U32_BE(p, idat_crc); p += 4;

    /* IEND */
    WRITE_U32_BE(p, 0); p += 4;
    uint8_t* iend_start = p;
    memcpy(p, "IEND", 4); p += 4;
    uint32_t iend_crc = update_crc(0, iend_start, 4);
    WRITE_U32_BE(p, iend_crc); p += 4;

    free(raw);
    *out_size = (size_t)(p - png);
    return png;
}

static uint8_t* render_plot_to_png(int w, int h, size_t* out_png_size) {
    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hbm = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hbm || !pBits) {
        if (hbm) DeleteObject(hbm);
        DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
        return NULL;
    }

    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, hbm);

    RECT rc = {0, 0, w, h};
    draw_graph(memDC, rc, true);

    GdiFlush();

    size_t row_stride = ((size_t)w * 3 + 3) & ~3;
    uint8_t* rgb = (uint8_t*)malloc((size_t)w * h * 3);
    if (!rgb) {
        SelectObject(memDC, oldBM);
        DeleteObject(hbm);
        DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
        return NULL;
    }

    const uint8_t* src = (const uint8_t*)pBits;
    for (int y = 0; y < h; y++) {
        const uint8_t* src_row = src + y * row_stride;
        uint8_t* dst_row = rgb + (size_t)y * w * 3;
        for (int x = 0; x < w; x++) {
            dst_row[x * 3 + 0] = src_row[x * 3 + 2]; // R
            dst_row[x * 3 + 1] = src_row[x * 3 + 1]; // G
            dst_row[x * 3 + 2] = src_row[x * 3 + 0]; // B
        }
    }

    SelectObject(memDC, oldBM);
    DeleteObject(hbm);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);

    uint8_t* png = create_png_rgb24(rgb, w, h, out_png_size);
    free(rgb);
    return png;
}

/* In-Memory ZIP Writer */
typedef struct {
    char name[128];
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
    void* data;
} ZipEntry;

typedef struct {
    ZipEntry entries[32];
    int count;
} ZipWriter;

static void zip_add(ZipWriter* zw, const char* name, const void* data, size_t size) {
    if (zw->count >= 32) return;
    ZipEntry* e = &zw->entries[zw->count++];
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->size = (uint32_t)size;
    e->crc = update_crc(0, (const uint8_t*)data, size);
    e->data = malloc(size);
    if (e->data) memcpy(e->data, data, size);
}

static int zip_write_file(ZipWriter* zw, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) return 0;

    for (int i = 0; i < zw->count; i++) {
        ZipEntry* e = &zw->entries[i];
        e->offset = (uint32_t)ftell(fp);
        uint16_t name_len = (uint16_t)strlen(e->name);

        uint8_t lfh[30];
        memset(lfh, 0, sizeof(lfh));
        lfh[0] = 0x50; lfh[1] = 0x4B; lfh[2] = 0x03; lfh[3] = 0x04;
        lfh[4] = 20;
        memcpy(&lfh[14], &e->crc, 4);
        memcpy(&lfh[18], &e->size, 4);
        memcpy(&lfh[22], &e->size, 4);
        memcpy(&lfh[26], &name_len, 2);

        fwrite(lfh, 1, 30, fp);
        fwrite(e->name, 1, name_len, fp);
        if (e->size > 0 && e->data) fwrite(e->data, 1, e->size, fp);
    }

    uint32_t cd_offset = (uint32_t)ftell(fp);
    for (int i = 0; i < zw->count; i++) {
        ZipEntry* e = &zw->entries[i];
        uint16_t name_len = (uint16_t)strlen(e->name);

        uint8_t cdh[46];
        memset(cdh, 0, sizeof(cdh));
        cdh[0] = 0x50; cdh[1] = 0x4B; cdh[2] = 0x01; cdh[3] = 0x02;
        cdh[4] = 20; cdh[6] = 20;
        memcpy(&cdh[16], &e->crc, 4);
        memcpy(&cdh[20], &e->size, 4);
        memcpy(&cdh[24], &e->size, 4);
        memcpy(&cdh[28], &name_len, 2);
        memcpy(&cdh[42], &e->offset, 4);

        fwrite(cdh, 1, 46, fp);
        fwrite(e->name, 1, name_len, fp);
    }

    uint32_t cd_size = (uint32_t)ftell(fp) - cd_offset;
    uint8_t eocd[22];
    memset(eocd, 0, sizeof(eocd));
    eocd[0] = 0x50; eocd[1] = 0x4B; eocd[2] = 0x05; eocd[3] = 0x06;
    uint16_t total_entries = (uint16_t)zw->count;
    memcpy(&eocd[8], &total_entries, 2);
    memcpy(&eocd[10], &total_entries, 2);
    memcpy(&eocd[12], &cd_size, 4);
    memcpy(&eocd[16], &cd_offset, 4);

    fwrite(eocd, 1, 22, fp);
    fclose(fp);

    for (int i = 0; i < zw->count; i++) {
        if (zw->entries[i].data) free(zw->entries[i].data);
    }
    return 1;
}

/* Dynamic string buffer for sheet XML */
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf* sb) {
    sb->cap = 16384;
    sb->len = 0;
    sb->data = (char*)malloc(sb->cap);
    if (sb->data) sb->data[0] = '\0';
}

static void sb_append(StrBuf* sb, const char* str) {
    size_t slen = strlen(str);
    if (sb->len + slen + 1 > sb->cap) {
        while (sb->len + slen + 1 > sb->cap) sb->cap *= 2;
        sb->data = (char*)realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, str, slen + 1);
    sb->len += slen;
}

static void sb_printf(StrBuf* sb, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sb_append(sb, buf);
}

static void xml_escape(const char* in, char* out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 6 < out_sz; i++) {
        if (in[i] == '&') { strcpy(&out[j], "&amp;"); j += 5; }
        else if (in[i] == '<') { strcpy(&out[j], "&lt;"); j += 4; }
        else if (in[i] == '>') { strcpy(&out[j], "&gt;"); j += 4; }
        else if (in[i] == '"') { strcpy(&out[j], "&quot;"); j += 6; }
        else if (in[i] == '\'') { strcpy(&out[j], "&apos;"); j += 6; }
        else { out[j++] = in[i]; }
    }
    out[j] = '\0';
}

static bool export_to_excel(HWND hwnd, const char* filepath) {
    /* 1. Render Graph to PNG */
    size_t png_size = 0;
    uint8_t* png_data = render_plot_to_png(1000, 600, &png_size);
    if (!png_data || png_size == 0) {
        MessageBoxA(hwnd, "Kunne ikke generere bilde av grafen.", "Eksportfeil", MB_ICONERROR);
        return false;
    }

    /* 2. Build OpenXML Worksheet XML */
    StrBuf sb;
    sb_init(&sb);
    sb_append(&sb, 
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\r\n"
        "  <cols>\r\n"
        "    <col min=\"1\" max=\"1\" width=\"18\" customWidth=\"1\"/>\r\n"
        "    <col min=\"2\" max=\"2\" width=\"20\" customWidth=\"1\"/>\r\n"
        "  </cols>\r\n"
        "  <sheetData>\r\n");

    char esc_title[256], esc_func[256], esc_xlab[256], esc_ylab[256];
    xml_escape(g_app.title_str, esc_title, sizeof(esc_title));
    xml_escape(g_app.func_str, esc_func, sizeof(esc_func));
    xml_escape(g_app.xlabel_str, esc_xlab, sizeof(esc_xlab));
    xml_escape(g_app.ylabel_str, esc_ylab, sizeof(esc_ylab));

    sb_printf(&sb, "    <row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Graf-tittel:</t></is></c><c r=\"B1\" t=\"inlineStr\"><is><t>%s</t></is></c></row>\r\n", esc_title);
    sb_printf(&sb, "    <row r=\"2\"><c r=\"A2\" t=\"inlineStr\"><is><t>Funksjon f(x):</t></is></c><c r=\"B2\" t=\"inlineStr\"><is><t>%s</t></is></c></row>\r\n", esc_func);
    sb_printf(&sb, "    <row r=\"3\"><c r=\"A3\" t=\"inlineStr\"><is><t>X-omr&#229;de:</t></is></c><c r=\"B3\" t=\"inlineStr\"><is><t>[%.4f, %.4f]</t></is></c></row>\r\n", g_app.x_min, g_app.x_max);
    sb_printf(&sb, "    <row r=\"4\"><c r=\"A4\" t=\"inlineStr\"><is><t>Y-omr&#229;de:</t></is></c><c r=\"B4\" t=\"inlineStr\"><is><t>[%.4f, %.4f]</t></is></c></row>\r\n", g_app.y_min, g_app.y_max);
    sb_printf(&sb, "    <row r=\"6\"><c r=\"A6\" t=\"inlineStr\"><is><t>%s (x)</t></is></c><c r=\"B6\" t=\"inlineStr\"><is><t>%s (f(x))</t></is></c></row>\r\n", esc_xlab, esc_ylab);

    int n_points = 1000;
    double dx = (g_app.x_max - g_app.x_min) / (n_points - 1);
    for (int i = 0; i < n_points; i++) {
        double mx = g_app.x_min + i * dx;
        double my = eval_ast(g_app.ast, mx);
        int row_idx = 7 + i;

        if (isnan(my) || isinf(my)) {
            sb_printf(&sb, "    <row r=\"%d\"><c r=\"A%d\"><v>%.6g</v></c><c r=\"B%d\" t=\"inlineStr\"><is><t>Udefinert</t></is></c></row>\r\n",
                      row_idx, row_idx, mx, row_idx);
        } else {
            sb_printf(&sb, "    <row r=\"%d\"><c r=\"A%d\"><v>%.6g</v></c><c r=\"B%d\"><v>%.6g</v></c></row>\r\n",
                      row_idx, row_idx, mx, row_idx, my);
        }
    }

    sb_append(&sb, "  </sheetData>\r\n  <drawing r:id=\"rId1\"/>\r\n</worksheet>\r\n");

    /* 3. Static OpenXML parts */
    const char* content_types = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\r\n"
        "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\r\n"
        "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\r\n"
        "  <Default Extension=\"png\" ContentType=\"image/png\"/>\r\n"
        "  <Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\r\n"
        "  <Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\r\n"
        "  <Override PartName=\"/xl/drawings/drawing1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/>\r\n"
        "</Types>";

    const char* root_rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\r\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\r\n"
        "</Relationships>";

    const char* wb_rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\r\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\r\n"
        "</Relationships>";

    const char* workbook =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\r\n"
        "  <sheets>\r\n"
        "    <sheet name=\"Funksjonsdata\" sheetId=\"1\" r:id=\"rId1\"/>\r\n"
        "  </sheets>\r\n"
        "</workbook>";

    const char* sheet_rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\r\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing1.xml\"/>\r\n"
        "</Relationships>";

    const char* drawing_rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\r\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"../media/image1.png\"/>\r\n"
        "</Relationships>";

    const char* drawing =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\r\n"
        "  <xdr:twoCellAnchor editAs=\"oneCell\">\r\n"
        "    <xdr:from><xdr:col>3</xdr:col><xdr:colOff>120000</xdr:colOff><xdr:row>1</xdr:row><xdr:rowOff>80000</xdr:rowOff></xdr:from>\r\n"
        "    <xdr:to><xdr:col>15</xdr:col><xdr:colOff>300000</xdr:colOff><xdr:row>27</xdr:row><xdr:rowOff>80000</xdr:rowOff></xdr:to>\r\n"
        "    <xdr:pic>\r\n"
        "      <xdr:nvPicPr>\r\n"
        "        <xdr:cNvPr id=\"2\" name=\"Funksjonsgraf\"/>\r\n"
        "        <xdr:cNvPicPr><a:picLocks noChangeAspect=\"1\"/></xdr:cNvPicPr>\r\n"
        "      </xdr:nvPicPr>\r\n"
        "      <xdr:blipFill>\r\n"
        "        <a:blip r:embed=\"rId1\"/>\r\n"
        "        <a:stretch><a:fillRect/></a:stretch>\r\n"
        "      </xdr:blipFill>\r\n"
        "      <xdr:spPr>\r\n"
        "        <a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom>\r\n"
        "      </xdr:spPr>\r\n"
        "    </xdr:pic>\r\n"
        "    <xdr:clientData/>\r\n"
        "  </xdr:twoCellAnchor>\r\n"
        "</xdr:wsDr>";

    /* 4. Pack into ZIP (.xlsx) */
    ZipWriter zw;
    memset(&zw, 0, sizeof(zw));

    zip_add(&zw, "[Content_Types].xml", content_types, strlen(content_types));
    zip_add(&zw, "_rels/.rels", root_rels, strlen(root_rels));
    zip_add(&zw, "xl/_rels/workbook.xml.rels", wb_rels, strlen(wb_rels));
    zip_add(&zw, "xl/workbook.xml", workbook, strlen(workbook));
    zip_add(&zw, "xl/worksheets/_rels/sheet1.xml.rels", sheet_rels, strlen(sheet_rels));
    zip_add(&zw, "xl/drawings/_rels/drawing1.xml.rels", drawing_rels, strlen(drawing_rels));
    zip_add(&zw, "xl/drawings/drawing1.xml", drawing, strlen(drawing));
    zip_add(&zw, "xl/worksheets/sheet1.xml", sb.data, sb.len);
    zip_add(&zw, "xl/media/image1.png", png_data, png_size);

    int ok = zip_write_file(&zw, filepath);

    free(png_data);
    free(sb.data);

    if (ok) {
        char msg[600];
        snprintf(msg, sizeof(msg),
            "Excel-filen ble vellykket lagret til:\n%s\n\n"
            "Innhold:\n"
            "  - 1000 datapunkter for x og f(x)\n"
            "  - Tittel, formel og akseopplysninger\n"
            "  - Innebygd bilde av funksjonsgrafen\n\n"
            "Vil du åpne filen i Excel nå?", filepath);
        if (MessageBoxA(hwnd, msg, "Eksport til Excel fullført", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            ShellExecuteA(hwnd, "open", filepath, NULL, NULL, SW_SHOWNORMAL);
        }
        return true;
    } else {
        MessageBoxA(hwnd, "Feil under skriving av Excel-filen.", "Eksportfeil", MB_ICONERROR);
        return false;
    }
}

static bool export_to_png_file(HWND hwnd, const char* filepath) {
    size_t png_size = 0;
    uint8_t* png_data = render_plot_to_png(1200, 750, &png_size);
    if (!png_data || png_size == 0) {
        MessageBoxA(hwnd, "Kunne ikke generere bilde av grafen.", "Eksportfeil", MB_ICONERROR);
        return false;
    }
    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        free(png_data);
        MessageBoxA(hwnd, "Kunne ikke åpne filen for skriving.", "Eksportfeil", MB_ICONERROR);
        return false;
    }
    fwrite(png_data, 1, png_size, fp);
    fclose(fp);
    free(png_data);

    char msg[MAX_PATH + 100];
    snprintf(msg, sizeof(msg), "Grafen ble vellykket lagret som bilde til:\n%s", filepath);
    MessageBoxA(hwnd, msg, "Lagret bilde", MB_OK | MB_ICONINFORMATION);
    return true;
}

static void create_main_menu(HWND hwnd) {
    HMENU hMenuBar  = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    HMENU hViewMenu = CreatePopupMenu();
    HMENU hHelpMenu = CreatePopupMenu();

    /* Fil-meny (Standard Windows Lagre / Lagre som) */
    AppendMenuA(hFileMenu, MF_STRING, ID_FILE_SAVE,       "&Lagre...\tCtrl+S");
    AppendMenuA(hFileMenu, MF_STRING, ID_FILE_SAVE_EXCEL, "Lagre som &Excel (.xlsx)...");
    AppendMenuA(hFileMenu, MF_STRING, ID_FILE_SAVE_PNG,   "Lagre som &bilde (.png)...");
    AppendMenuA(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hFileMenu, MF_STRING, ID_FILE_EXIT,       "&Avslutt\tAlt+F4");

    /* Vis-meny */
    AppendMenuA(hViewMenu, MF_STRING, ID_BTN_ZOOM_BOTH_IN,  "Zoom &inn (begge akser)\t+");
    AppendMenuA(hViewMenu, MF_STRING, ID_BTN_ZOOM_BOTH_OUT, "Zoom &ut (begge akser)\t-");
    AppendMenuA(hViewMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hViewMenu, MF_STRING, ID_BTN_ZOOM_X_IN,     "Zoom inn &X-akse\tX");
    AppendMenuA(hViewMenu, MF_STRING, ID_BTN_ZOOM_X_OUT,    "Zoom ut X-akse\tShift+X");
    AppendMenuA(hViewMenu, MF_STRING, ID_BTN_ZOOM_Y_IN,     "Zoom inn &Y-akse\tY");
    AppendMenuA(hViewMenu, MF_STRING, ID_BTN_ZOOM_Y_OUT,    "Zoom ut Y-akse\tShift+Y");
    AppendMenuA(hViewMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hViewMenu, MF_STRING, ID_BTN_RESET,         "&Nullstill visning\tR");

    /* Hjelp-meny */
    AppendMenuA(hHelpMenu, MF_STRING, ID_HELP_ABOUT,        "&Om Funksjonsplotter...");

    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, "&Fil");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, "&Vis");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, "&Hjelp");

    SetMenu(hwnd, hMenuBar);
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

    CreateWindowA("STATIC", "Tittel:", WS_CHILD | WS_VISIBLE, 252, 12, 38, 22, hwnd, NULL, hInst, NULL);
    g_app.hEditTitle = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_app.title_str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 292, 10, 150, 24, hwnd, (HMENU)ID_EDIT_TITLE, hInst, NULL);

    CreateWindowA("STATIC", "X-Akse:", WS_CHILD | WS_VISIBLE, 452, 12, 48, 22, hwnd, NULL, hInst, NULL);
    g_app.hEditXLabel = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_app.xlabel_str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 502, 10, 90, 24, hwnd, (HMENU)ID_EDIT_XLABEL, hInst, NULL);

    CreateWindowA("STATIC", "Y-Akse:", WS_CHILD | WS_VISIBLE, 602, 12, 48, 22, hwnd, NULL, hInst, NULL);
    g_app.hEditYLabel = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_app.ylabel_str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 652, 10, 90, 24, hwnd, (HMENU)ID_EDIT_YLABEL, hInst, NULL);

    g_app.hBtnPlot = CreateWindowA("BUTTON", "Tegn graf", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 755, 9, 95, 26, hwnd, (HMENU)ID_BTN_PLOT, hInst, NULL);

    /* Row 2: Zoom Controls & Excel Export */
    CreateWindowA("STATIC", "Zoom:", WS_CHILD | WS_VISIBLE, 12, 48, 42, 22, hwnd, NULL, hInst, NULL);
    g_app.hBtnZoomXIn     = CreateWindowA("BUTTON", "X +", WS_CHILD | WS_VISIBLE, 56, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_X_IN, hInst, NULL);
    g_app.hBtnZoomXOut    = CreateWindowA("BUTTON", "X -", WS_CHILD | WS_VISIBLE, 108, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_X_OUT, hInst, NULL);

    g_app.hBtnZoomYIn     = CreateWindowA("BUTTON", "Y +", WS_CHILD | WS_VISIBLE, 168, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_Y_IN, hInst, NULL);
    g_app.hBtnZoomYOut    = CreateWindowA("BUTTON", "Y -", WS_CHILD | WS_VISIBLE, 220, 45, 48, 25, hwnd, (HMENU)ID_BTN_ZOOM_Y_OUT, hInst, NULL);

    g_app.hBtnZoomBothIn  = CreateWindowA("BUTTON", "Begge +", WS_CHILD | WS_VISIBLE, 280, 45, 68, 25, hwnd, (HMENU)ID_BTN_ZOOM_BOTH_IN, hInst, NULL);
    g_app.hBtnZoomBothOut = CreateWindowA("BUTTON", "Begge -", WS_CHILD | WS_VISIBLE, 352, 45, 68, 25, hwnd, (HMENU)ID_BTN_ZOOM_BOTH_OUT, hInst, NULL);

    g_app.hBtnReset       = CreateWindowA("BUTTON", "Nullstill visning", WS_CHILD | WS_VISIBLE, 430, 45, 105, 25, hwnd, (HMENU)ID_BTN_RESET, hInst, NULL);

    g_app.hBtnExcel       = CreateWindowA("BUTTON", "Eksporter til Excel (.xlsx)", WS_CHILD | WS_VISIBLE, 550, 45, 185, 25, hwnd, (HMENU)ID_BTN_EXCEL, hInst, NULL);

    /* Subclass edit controls so pressing Enter automatically plots */
    g_oldEditProc = (WNDPROC)SetWindowLongPtr(g_app.hEditFunc, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    SetWindowLongPtr(g_app.hEditTitle,  GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    SetWindowLongPtr(g_app.hEditXLabel, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    SetWindowLongPtr(g_app.hEditYLabel, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    HWND controls[] = {
        g_app.hEditFunc, g_app.hEditTitle, g_app.hEditXLabel, g_app.hEditYLabel,
        g_app.hBtnPlot, g_app.hBtnZoomXIn, g_app.hBtnZoomXOut, g_app.hBtnZoomYIn,
        g_app.hBtnZoomYOut, g_app.hBtnZoomBothIn, g_app.hBtnZoomBothOut, g_app.hBtnReset,
        g_app.hBtnExcel
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
            create_main_menu(hwnd);
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
                case ID_FILE_SAVE:
                case ID_FILE_SAVE_EXCEL:
                case ID_BTN_EXCEL: {
                    char szFile[MAX_PATH] = "funksjonsgraf.xlsx";
                    OPENFILENAMEA ofn;
                    memset(&ofn, 0, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFilter = "Excel-arbeidsbok (*.xlsx)\0*.xlsx\0Alle filer (*.*)\0*.*\0";
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrDefExt = "xlsx";
                    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

                    if (GetSaveFileNameA(&ofn)) {
                        export_to_excel(hwnd, szFile);
                    }
                    break;
                }
                case ID_FILE_SAVE_PNG: {
                    char szFile[MAX_PATH] = "funksjonsgraf.png";
                    OPENFILENAMEA ofn;
                    memset(&ofn, 0, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFilter = "PNG-bilde (*.png)\0*.png\0Alle filer (*.*)\0*.*\0";
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrDefExt = "png";
                    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

                    if (GetSaveFileNameA(&ofn)) {
                        export_to_png_file(hwnd, szFile);
                    }
                    break;
                }
                case ID_FILE_EXIT:
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
                case ID_HELP_ABOUT:
                    MessageBoxA(hwnd,
                        "C Interaktiv Funksjonsplotter\n\n"
                        "Menylagring:\n"
                        "  - Fil -> Lagre... (Ctrl+S)\n"
                        "    Lagrer regneark i Excel-format (.xlsx) med:\n"
                        "    1. Tabell med 1000 datapunkter for x og f(x)\n"
                        "    2. Grafens tittel, formel og akseopplysninger\n"
                        "    3. Innebygd bilde av funksjonsgrafen ved siden av tabellen\n\n"
                        "  - Fil -> Lagre som bilde (.png)...\n"
                        "    Lagrer et høyoppløselig PNG-bilde av grafen.\n\n"
                        "Navigasjon:\n"
                        "  - Dra grafen med musen for å panorere\n"
                        "  - Rullehjul: Zoom begge akser\n"
                        "  - Shift + Rullehjul: Zoom X-akse\n"
                        "  - Ctrl + Rullehjul: Zoom Y-akse\n"
                        "  - R: Nullstill visning\n\n"
                        "Støttede matematiske funksjoner:\n"
                        "  sin, cos, tan, asin, acos, atan, sinh, cosh, tanh,\n"
                        "  sqrt, cbrt, exp, ln, log10, abs, floor, ceil, pi, e",
                        "Om Funksjonsplotter", MB_OK | MB_ICONINFORMATION);
                    break;
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            double factor = (delta > 0) ? 0.85 : (1.0 / 0.85);

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
            RECT gr = {85, 122, rcClient.right - 40, rcClient.bottom - 71};
            if (pt.x >= gr.left && pt.x <= gr.right && pt.y >= gr.top && pt.y <= gr.bottom) {
                cx = to_math_x(pt.x, gr);
                cy = to_math_y(pt.y, gr);
            }

            if (shift) {
                zoom_viewport(factor, 1.0, cx, cy);
            } else if (ctrl) {
                zoom_viewport(1.0, factor, cx, cy);
            } else {
                zoom_viewport(factor, factor, cx, cy);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            RECT gr = {85, 122, rcClient.right - 40, rcClient.bottom - 71};

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
            RECT gr = {85, 122, rcClient.right - 40, rcClient.bottom - 71};

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
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'S') {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_FILE_SAVE, 0), 0);
                return 0;
            }

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

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

            draw_graph(memDC, rcClient, false);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

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

    memset(&g_app, 0, sizeof(g_app));
    g_app.x_min = -10.0;
    g_app.x_max =  10.0;
    g_app.y_min = -10.0;
    g_app.y_max =  10.0;

    strncpy(g_app.func_str, "sin(x) * x / 2", sizeof(g_app.func_str) - 1);
    strncpy(g_app.title_str, "f(x) = sin(x) * x / 2", sizeof(g_app.title_str) - 1);
    strncpy(g_app.xlabel_str, "Tid / Posisjon (x)", sizeof(g_app.xlabel_str) - 1);
    strncpy(g_app.ylabel_str, "Amplitude f(x)", sizeof(g_app.ylabel_str) - 1);

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
        MessageBoxA(NULL, "Kunne ikke registrere vindusklasse.", "Feil", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExA(
        0,
        "CFunctionPlotterWnd",
        "C Interaktiv Funksjonsplotter - f(x)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1020, 700,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Kunne ikke opprette hovedvindu.", "Feil", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
