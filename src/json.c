/* The JSON writer of `langc report`. See json.h. */
#include <stdio.h>
#include <string.h>

#include "json.h"

void json_out_init(JsonOut *out, Arena *arena, Diag *diag, const char *def) {
  out->arena = arena;
  out->diag = diag;
  out->def = def;
  out->data = NULL;
  out->len = 0;
  out->cap = 0;
}

int json_put(JsonOut *out, const char *text, size_t n) {
  if (out->len + n + 1 > out->cap) {
    size_t cap = out->cap == 0 ? 4096 : out->cap;
    while (cap < out->len + n + 1) cap *= 2;
    char *data = arena_alloc(out->arena, cap);
    if (data == NULL) return diag_fail(out->diag, "OOM", out->def, "the arena limit is reached");
    if (out->len > 0) memcpy(data, out->data, out->len);
    out->data = data;
    out->cap = cap;
  }
  memcpy(out->data + out->len, text, n);
  out->len += n;
  out->data[out->len] = '\0';
  return 1;
}

int json_put_text(JsonOut *out, const char *text) {
  return json_put(out, text, strlen(text));
}

int json_put_string(JsonOut *out, const char *text) {
  if (text == NULL) return json_put_text(out, "null");
  int ok = json_put(out, "\"", 1);
  for (const char *s = text; ok && *s != '\0';) {
    char esc[JSON_ESCAPE_MAX];
    size_t len;
    s += json_escape(s, esc, &len);
    ok = json_put(out, esc, len);
  }
  return ok && json_put(out, "\"", 1);
}

/* Adds N as a JSON number. */
static int json_put_size(JsonOut *out, size_t n) {
  char buf[32];
  int len = snprintf(buf, sizeof buf, "%zu", n);
  return json_put(out, buf, (size_t)len);
}

/* Adds ",", except before item 0. */
static int json_put_comma(JsonOut *out, size_t i) {
  return i == 0 || json_put_text(out, ",");
}

/* Adds the facts as [{"fact":F,"guard":G},...]. */
static int json_put_facts(JsonOut *out, const ReportFact *facts, size_t count) {
  int ok = json_put_text(out, "[");
  for (size_t i = 0; ok && i < count; i++) {
    ok = json_put_comma(out, i) && json_put_text(out, "{\"fact\":") && json_put_string(out, facts[i].fact) &&
         json_put_text(out, ",\"guard\":") && json_put_string(out, facts[i].guard) && json_put_text(out, "}");
  }
  return ok && json_put_text(out, "]");
}

/* Adds stage I as {"index","steps","lock","scan","guards"}. The steps array
   has the step text, or nothing at stage 0. */
static int json_put_stage(JsonOut *out, size_t i, const ReportStage *s) {
  return json_put_text(out, "{\"index\":") && json_put_size(out, i) && json_put_text(out, ",\"steps\":[") &&
         (s->step == NULL || json_put_string(out, s->step)) && json_put_text(out, "],\"lock\":") &&
         json_put_string(out, s->lock) && json_put_text(out, ",\"scan\":") && json_put_string(out, s->scan) &&
         json_put_text(out, ",\"guards\":") && json_put_facts(out, s->guards, s->guard_count) && json_put_text(out, "}");
}

/* Adds a segment as {"stages":[FIRST..LAST],"live":[the apps live at
   FIRST]}. */
static int json_put_segment(JsonOut *out, const Report *r, const ReportSegment *seg) {
  int ok = json_put_text(out, "{\"stages\":[");
  for (size_t i = seg->first; ok && i <= seg->last; i++) ok = json_put_comma(out, i - seg->first) && json_put_size(out, i);
  ok = ok && json_put_text(out, "],\"live\":[");
  size_t live = 0;
  for (size_t a = 0; ok && a < r->app_count; a++) {
    if (r->cells[a * r->stage_count + seg->first].live) {
      ok = json_put_comma(out, live++) && json_put_string(out, r->apps[a]);
    }
  }
  return ok && json_put_text(out, "]}");
}

/* Adds the verdict of a matrix row: "verdict", then "code" and "fix" for
   refused, "fact" and "guard" for runtime. */
static int json_put_verdict(JsonOut *out, const ReportCell *c) {
  switch (c->verdict) {
    case REPORT_COMPATIBLE:
      return json_put_text(out, ",\"verdict\":\"compatible\"");
    case REPORT_REFUSED:
      return json_put_text(out, ",\"verdict\":\"refused\",\"code\":") && json_put_string(out, c->code) &&
             json_put_text(out, ",\"fix\":") && json_put_string(out, c->fix);
    case REPORT_RUNTIME:
      return json_put_text(out, ",\"verdict\":\"runtime\",\"fact\":") && json_put_string(out, c->fact.fact) &&
             json_put_text(out, ",\"guard\":") && json_put_string(out, c->fact.guard);
  }
  return 0;
}

int json_report(JsonOut *out, const Report *r, const char *progress) {
  int ok = json_put_text(out, "{\"evolve\":1,\"protocol\":") && json_put_string(out, r->protocol) &&
           json_put_text(out, ",\"stages\":[");
  for (size_t i = 0; ok && i < r->stage_count; i++) ok = json_put_comma(out, i) && json_put_stage(out, i, &r->stages[i]);
  ok = ok && json_put_text(out, "],\"apps\":[");
  for (size_t a = 0; ok && a < r->app_count; a++) ok = json_put_comma(out, a) && json_put_string(out, r->apps[a]);
  ok = ok && json_put_text(out, "],\"segments\":[");
  for (size_t s = 0; ok && s < r->segment_count; s++) {
    ok = json_put_comma(out, s) && json_put_segment(out, r, &r->segments[s]);
  }
  /* App order, then stage order: row K is app K / STAGE_COUNT at stage
     K % STAGE_COUNT. */
  ok = ok && json_put_text(out, "],\"matrix\":[");
  for (size_t k = 0; ok && k < r->app_count * r->stage_count; k++) {
    const ReportCell *c = &r->cells[k];
    ok = json_put_comma(out, k) && json_put_text(out, "{\"app\":") && json_put_string(out, r->apps[k / r->stage_count]) &&
         json_put_text(out, ",\"stage\":") && json_put_size(out, k % r->stage_count) &&
         json_put_text(out, c->live ? ",\"live\":true" : ",\"live\":false") && json_put_verdict(out, c) &&
         json_put_text(out, "}");
  }
  return ok && json_put_text(out, "],\"progress\":") && json_put_text(out, progress) && json_put_text(out, "}\n");
}
