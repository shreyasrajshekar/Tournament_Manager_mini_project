#include "server/report.h"
#include "core/bracket.h"
#include "core/ranking.h"
#include "core/predict.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static void esc(StrBuf *o, const char *s) {
    for (; *s; s++) switch (*s) {
        case '&': sb_puts(o, "&amp;"); break; case '<': sb_puts(o, "&lt;"); break;
        case '>': sb_puts(o, "&gt;"); break; case '"': sb_puts(o, "&quot;"); break;
        default: sb_printf(o, "%c", *s);
    }
}

void report_html(Tournament *t, StrBuf *o) {
    time_t now = time(NULL); char when[40]; strftime(when, sizeof when, "%d %b %Y, %H:%M", localtime(&now));
    int played = (int)t->undo.size;
    sb_puts(o, "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>");
    esc(o, t->name); sb_puts(o, " - Tournament Report</title><style>"
        "body{font:15px/1.5 Georgia,'Times New Roman',serif;color:#1c2333;max-width:820px;margin:32px auto;padding:0 24px}"
        "h1{font-size:30px;margin:0 0 2px}h2{font-size:19px;margin:30px 0 8px;border-bottom:2px solid #1f3a8a;padding-bottom:3px;color:#1f3a8a}"
        ".sub{color:#5d6272}.champ{margin:18px 0;padding:16px 20px;border:3px double #1f3a8a;text-align:center;background:#fffbe0}"
        ".champ b{display:block;font-size:28px;color:#1f3a8a}table{border-collapse:collapse;width:100%;font-size:14px}"
        "th,td{padding:5px 8px;border-bottom:1px solid #d6d9e3;text-align:left}th{background:#eef1fa}td.n,th.n{text-align:right}"
        ".w{font-weight:700}.btn{float:right;padding:6px 14px;border:1px solid #1f3a8a;background:#fff;cursor:pointer;font:inherit}"
        "@media print{.btn{display:none}body{margin:0}}</style></head><body>"
        "<button class=\"btn\" onclick=\"window.print()\">Print / save as PDF</button><h1>");
    esc(o, t->name);
    sb_printf(o, "</h1><div class=\"sub\">Tournament report &middot; generated %s &middot; %d clubs &middot; %d matches played</div>", when, (int)t->teams.size, played);
    if (t->champion) { sb_puts(o, "<div class=\"champ\">Champions<b>"); esc(o, t->champion->name); sb_puts(o, "</b></div>"); }
    else sb_puts(o, "<div class=\"champ\">Tournament not finished yet</div>");

    sb_puts(o, "<h2>Match results</h2><table><tr><th>#</th><th>Round</th><th>Match</th><th class=\"n\">Score</th><th>Winner</th></tr>");
    for (size_t i = 0; i < t->undo.size; i++) {
        const Result *r = t->undo.data[i]; char rn[32]; bracket_round_name(t, r->round, rn, sizeof rn);
        sb_printf(o, "<tr><td>%d</td><td>%s</td><td>", r->matchId, rn);
        esc(o, r->nameA); sb_puts(o, " vs "); esc(o, r->nameB);
        sb_printf(o, "</td><td class=\"n\">%d - %d</td><td class=\"w\">", r->scoreA, r->scoreB);
        esc(o, r->winnerId == r->teamA ? r->nameA : r->nameB); sb_puts(o, "</td></tr>");
    }
    sb_puts(o, "</table><h2>Final standings</h2><table><tr><th>#</th><th>Club</th><th class=\"n\">Seed</th><th class=\"n\">W</th><th class=\"n\">L</th><th class=\"n\">GF</th><th class=\"n\">GA</th><th class=\"n\">Rating</th></tr>");
    Team *rk[MAX_TEAMS]; int k = ranking_standings(t, rk, MAX_TEAMS);
    for (int i = 0; i < k; i++) {
        sb_printf(o, "<tr><td>%d</td><td>", i + 1); esc(o, rk[i]->name);
        sb_printf(o, "</td><td class=\"n\">%d</td><td class=\"n\">%d</td><td class=\"n\">%d</td><td class=\"n\">%d</td><td class=\"n\">%d</td><td class=\"n\">%.0f</td></tr>",
                  rk[i]->seed, rk[i]->wins, rk[i]->losses, rk[i]->pointsFor, rk[i]->pointsAgainst, rk[i]->rating);
    }
    sb_puts(o, "</table>");

    Fan *fl[MAX_FANS]; int nf = fan_leaderboard(t, fl, MAX_FANS);
    if (nf) {
        sb_puts(o, "<h2>Fan prediction leaderboard</h2><table><tr><th>#</th><th>Fan</th><th class=\"n\">Points</th><th class=\"n\">Predictions</th></tr>");
        for (int i = 0; i < nf; i++) {
            sb_printf(o, "<tr><td>%d</td><td>", i + 1); esc(o, fl[i]->name);
            sb_printf(o, "</td><td class=\"n\">%d</td><td class=\"n\">%d</td></tr>", fan_points(t, fl[i]), fan_total(t, fl[i]));
        }
        sb_puts(o, "</table>");
    }
    sb_puts(o, "</body></html>");
}
