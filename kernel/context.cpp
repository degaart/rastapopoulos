#include "context.h"
#include "string.h"
#include "util.h"

struct iret_t {
    uint32_t cs;
    uint32_t ds;
    uint32_t ss;
    uint32_t cr3;
    struct regs_t regs;
};

extern "C" void perform_iret(iret_t* ctx);

void switch_context(context_t* ctx) {
    iret_t params;
    bzero(&params, sizeof(params));

    params.cs = ctx->cs;
    params.ds = ctx->ds;
    params.ss = ctx->ss;
    params.cr3 = ctx->pagedir->physical();
    params.regs = ctx->regs;
    VMM::set_pagedir(ctx->pagedir);
    perform_iret(&params);
    halt();
}

