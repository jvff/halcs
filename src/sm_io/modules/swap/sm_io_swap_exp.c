/*
 * Copyright (C) 2014 LNLS (www.lnls.br)
 * Author: Lucas Russo <lucas.russo@lnls.br>
 *
 * Released according to the GNU GPL, version 3 or any later version.
 */

#include "halcs_server.h"
/* Private headers */
#include "sm_io_swap_codes.h"
#include "sm_io_swap_defaults.h"
#include "sm_io_swap_useful_macros.h"
#include "sm_io_swap_exports.h"
#include "sm_io_swap_core.h"
#include "sm_io_swap_exp.h"
#include "hw/wb_bpm_swap_regs.h"

/* Undef ASSERT_ALLOC to avoid conflicting with other ASSERT_ALLOC */
#ifdef ASSERT_TEST
#undef ASSERT_TEST
#endif
#define ASSERT_TEST(test_boolean, err_str, err_goto_label, /* err_core */ ...) \
    ASSERT_HAL_TEST(test_boolean, SM_IO, "[sm_io:swap_exp]",    \
            err_str, err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef ASSERT_ALLOC
#undef ASSERT_ALLOC
#endif
#define ASSERT_ALLOC(ptr, err_goto_label, /* err_core */ ...)   \
    ASSERT_HAL_ALLOC(ptr, SM_IO, "[sm_io:swap_exp]",            \
            smio_err_str(SMIO_ERR_ALLOC),                       \
            err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef CHECK_ERR
#undef CHECK_ERR
#endif
#define CHECK_ERR(err, err_type)                                \
    CHECK_HAL_ERR(err, SM_IO, "[sm_io:swap_exp]",               \
            smio_err_str (err_type))

/************************************************************/
/*************    Specific SWAP operations     **************/
/************************************************************/
/* 0: Outputs matched. No signal
 * 1: Outputs in direct path
 * 2: Outputs in inverted path
 * 3: Outputs switching
 * */
#define SW_MIN                                  0
#define SW_MAX                                  3

#define BPM_SWAP_CTRL_MODE_GLOBAL_MASK          (BPM_SWAP_CTRL_MODE1_MASK   | BPM_SWAP_CTRL_MODE2_MASK)
#define BPM_SWAP_CTRL_MODE_GLOBAL_W(val)        (BPM_SWAP_CTRL_MODE1_W(val) | BPM_SWAP_CTRL_MODE2_W(val))
#define BPM_SWAP_CTRL_MODE_GLOBAL_R(val)        (BPM_SWAP_CTRL_MODE1_R(val) | BPM_SWAP_CTRL_MODE2_R(val))
RW_PARAM_FUNC(swap, sw) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, CTRL, MODE_GLOBAL, MULT_BIT_PARAM,
            SW_MIN, SW_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define HALCS_EN_MIN                           0 /* Switching enabled */
#define HALCS_EN_MAX                           1 /* Switching disabled */
RW_PARAM_FUNC(swap, sw_en) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, CTRL, CLK_SWAP_EN, SINGLE_BIT_PARAM,
            HALCS_EN_MIN, HALCS_EN_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define BPM_SWAP_DIV_F_MIN                      1
#define BPM_SWAP_DIV_F_MAX                      ((1<<16)-1)
RW_PARAM_FUNC(swap, div_clk) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, CTRL, SWAP_DIV_F, MULT_BIT_PARAM,
            BPM_SWAP_DIV_F_MIN, BPM_SWAP_DIV_F_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

/* Number of clock cycles between the switching and deswitching */
#define BPM_SWAP_SW_DLY_MIN                     0
#define BPM_SWAP_SW_DLY_MAX                     ((1<<16)-1)

#define BPM_SWAP_DLY_GLOBAL_MASK                (BPM_SWAP_DLY_1_MASK   | BPM_SWAP_DLY_2_MASK)
#define BPM_SWAP_DLY_GLOBAL_W(val)              (BPM_SWAP_DLY_1_W(val) | BPM_SWAP_DLY_2_W(val))
#define BPM_SWAP_DLY_GLOBAL_R(val)              (BPM_SWAP_DLY_1_R(val) | BPM_SWAP_DLY_2_R(val))
RW_PARAM_FUNC(swap, sw_dly) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, DLY, GLOBAL, MULT_BIT_PARAM,
            BPM_SWAP_SW_DLY_MIN, BPM_SWAP_SW_DLY_MAX, NO_CHK_FUNC, NO_FMT_FUNC,
            SET_FIELD);
}

#define BPM_SWAP_WDW_EN_MIN                     0 /* Windowing enabled */
#define BPM_SWAP_WDW_EN_MAX                     1 /* Windowing disabled */

#define BPM_SWAP_WDW_CTL_EN_GLOBAL              (BPM_SWAP_WDW_CTL_USE | BPM_SWAP_WDW_CTL_SWCLK_EXT)
RW_PARAM_FUNC(swap, wdw_en) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, WDW_CTL, EN_GLOBAL, SINGLE_BIT_PARAM,
            BPM_SWAP_WDW_EN_MIN, BPM_SWAP_WDW_EN_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define BPM_SWAP_WDW_DLY_MIN                    0
#define BPM_SWAP_WDW_DLY_MAX                    ((1<<16)-1)
RW_PARAM_FUNC(swap, wdw_dly) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, WDW_CTL, DLY, MULT_BIT_PARAM,
            BPM_SWAP_WDW_DLY_MIN, BPM_SWAP_WDW_DLY_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

/* SWAP gain functions.
 * These are a bit different from the others, in that the parameter passed to
 * them are neither a single bit nor a multi-bit paramter written to multiple
 * fields. Instead, the parameter is divided in 2 parts of 16-bits (16-bit lower
 * part is the direct path gain and the 16-bit upper part is the inverted path
 * gain) written as a whole in the specified register */

/* All gains have the same format, so we use the A gain for all of them */
#define RW_SWAP_GAIN_MIN                        1
#define RW_SWAP_GAIN_MAX                        ((1<<16)-1)
/* Custom parameter check */
static int _rw_bpm_swap_gain_chk (uint32_t gain)
{
    if (RW_SWAP_GAIN_LOWER_R(gain) < RW_SWAP_GAIN_MIN ||
            RW_SWAP_GAIN_LOWER_R(gain) > RW_SWAP_GAIN_MAX ||
            RW_SWAP_GAIN_UPPER_R(gain) < RW_SWAP_GAIN_MIN ||
            RW_SWAP_GAIN_UPPER_R(gain) > RW_SWAP_GAIN_MAX) {
        return PARAM_ERR;
    }

    return PARAM_OK;
}

rw_param_check_fp rw_bpm_swap_gain_chk_fp = _rw_bpm_swap_gain_chk;

#define BPM_SWAP_GAIN_MIN                       0
#define BPM_SWAP_GAIN_MAX                       ((1ULL<<32)-1)

#define BPM_SWAP_A_GLOBAL_MASK                  (BPM_SWAP_A_A_MASK      | BPM_SWAP_A_C_MASK)
#define BPM_SWAP_A_GLOBAL_W(val)                (val)
#define BPM_SWAP_A_GLOBAL_R(val)                (val)
RW_PARAM_FUNC(swap, gain_a) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, A, GLOBAL, MULT_BIT_PARAM,
            BPM_SWAP_GAIN_MIN, BPM_SWAP_GAIN_MAX, rw_bpm_swap_gain_chk_fp,
            NO_FMT_FUNC, SET_FIELD);
}

#define BPM_SWAP_B_GLOBAL_MASK                  (BPM_SWAP_B_B_MASK      | BPM_SWAP_B_D_MASK)
#define BPM_SWAP_B_GLOBAL_W(val)                (val)
#define BPM_SWAP_B_GLOBAL_R(val)                (val)
RW_PARAM_FUNC(swap, gain_b) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, B, GLOBAL, MULT_BIT_PARAM,
            BPM_SWAP_GAIN_MIN, BPM_SWAP_GAIN_MAX, rw_bpm_swap_gain_chk_fp,
            NO_FMT_FUNC, SET_FIELD);
}

#define BPM_SWAP_C_GLOBAL_MASK                  (BPM_SWAP_C_C_MASK      | BPM_SWAP_C_A_MASK)
#define BPM_SWAP_C_GLOBAL_W(val)                (val)
#define BPM_SWAP_C_GLOBAL_R(val)                (val)
RW_PARAM_FUNC(swap, gain_c) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, C, GLOBAL, MULT_BIT_PARAM,
            BPM_SWAP_GAIN_MIN, BPM_SWAP_GAIN_MAX, rw_bpm_swap_gain_chk_fp,
            NO_FMT_FUNC, SET_FIELD);
}

#define BPM_SWAP_D_GLOBAL_MASK                  (BPM_SWAP_D_D_MASK      | BPM_SWAP_D_B_MASK)
#define BPM_SWAP_D_GLOBAL_W(val)                (val)
#define BPM_SWAP_D_GLOBAL_R(val)                (val)
RW_PARAM_FUNC(swap, gain_d) {
    SET_GET_PARAM(swap, 0x0, BPM_SWAP, D, GLOBAL, MULT_BIT_PARAM,
            BPM_SWAP_GAIN_MIN, BPM_SWAP_GAIN_MAX, rw_bpm_swap_gain_chk_fp,
            NO_FMT_FUNC, SET_FIELD);
}

/* Exported function pointers */
const disp_table_func_fp swap_exp_fp [] = {
    RW_PARAM_FUNC_NAME(swap, sw),
    RW_PARAM_FUNC_NAME(swap, sw_en),
    RW_PARAM_FUNC_NAME(swap, div_clk),
    RW_PARAM_FUNC_NAME(swap, sw_dly),
    RW_PARAM_FUNC_NAME(swap, wdw_en),
    RW_PARAM_FUNC_NAME(swap, wdw_dly),
    RW_PARAM_FUNC_NAME(swap, gain_a),
    RW_PARAM_FUNC_NAME(swap, gain_b),
    RW_PARAM_FUNC_NAME(swap, gain_c),
    RW_PARAM_FUNC_NAME(swap, gain_d),
    NULL
};

/************************************************************/
/***************** Export methods functions *****************/
/************************************************************/

static smio_err_e _swap_do_op (void *owner, void *msg);

/* Attach an instance of sm_io to dev_io function pointer */
smio_err_e swap_attach (smio_t *self, devio_t *parent)
{
    (void) self;
    (void) parent;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Deattach an instance of sm_io to dev_io function pointer */
smio_err_e swap_deattach (smio_t *self)
{
    (void) self;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Export (register) sm_io to handle operations function pointer */
smio_err_e swap_export_ops (smio_t *self,
        const disp_op_t **smio_exp_ops)
{
    (void) self;
    (void) smio_exp_ops;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Unexport (unregister) sm_io to handle operations function pointer */
smio_err_e swap_unexport_ops (smio_t *self)
{
    (void) self;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Generic wrapper for receiving opcodes and arguments to specific funtions function pointer */
/* FIXME: Code repetition! _devio_do_smio_op () function does almost the same!!! */
smio_err_e _swap_do_op (void *owner, void *msg)
{
    (void) owner;
    (void) msg;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

smio_err_e swap_do_op (void *self, void *msg)
{
    return _swap_do_op (self, msg);
}

const smio_ops_t swap_ops = {
    .attach             = swap_attach,          /* Attach sm_io instance to dev_io */
    .deattach           = swap_deattach,        /* Deattach sm_io instance to dev_io */
    .export_ops         = swap_export_ops,      /* Export sm_io operations to dev_io */
    .unexport_ops       = swap_unexport_ops,    /* Unexport sm_io operations to dev_io */
    .do_op              = swap_do_op            /* Generic wrapper for handling specific operations */
};

/************************************************************/
/****************** Bootstrap Operations ********************/
/************************************************************/

smio_err_e swap_init (smio_t * self)
{
    DBE_DEBUG (DBG_SM_IO | DBG_LVL_TRACE, "[sm_io:swap_exp] Initializing swap\n");

    smio_err_e err = SMIO_SUCCESS;

    err = smio_set_id (self, SWAP_SDB_DEVID);
    ASSERT_TEST(err == SMIO_SUCCESS, "Could not set SMIO id", err_set_id);
    err = smio_set_name (self, SWAP_SDB_NAME);
    ASSERT_TEST(err == SMIO_SUCCESS, "Could not set SMIO name", err_set_name);

    /* Set SMIO ops pointers */
    err = smio_set_ops (self, &swap_ops);
    ASSERT_TEST(err == SMIO_SUCCESS, "Could not set SMIO operations",
            err_smio_set_ops);
    err = smio_set_thsafe_client_ops (self, &smio_thsafe_client_zmq_ops);
    ASSERT_TEST(err == SMIO_SUCCESS, "Could not set SMIO thsafe operations",
            err_smio_set_thsafe_ops);

    /* Fill the disp_op_t description structure with the callbacks. */

    /* disp_op_t structure is const and all of the functions performing on it
     * obviously receives a const argument, but here (and only on the SMIO
     * initialization) we need to make an exception if we want to keep the
     * functions' description and the function pointers separate */
    err = smio_init_exp_ops (self, (disp_op_t **) swap_exp_ops, swap_exp_fp);
    ASSERT_TEST(err == SMIO_SUCCESS, "Could not fill SMIO "
            "function descriptors with the callbacks", err_fill_desc);

    err = smio_set_exp_ops (self, swap_exp_ops);
    ASSERT_TEST(err == SMIO_SUCCESS, "Could not set SMIO exported operations",
            err_smio_set_exp_ops);

    /* Initialize specific structure */
    smio_swap_t *smio_handler = smio_swap_new (self);
    ASSERT_ALLOC(smio_handler, err_smio_handler_alloc, SMIO_ERR_ALLOC);
    err = smio_set_handler (self, smio_handler);
    ASSERT_TEST(err == SMIO_SUCCESS, "Could not set SMIO handler",
            err_smio_set_handler);

    return err;

err_smio_set_handler:
    smio_swap_destroy (&smio_handler);
err_smio_handler_alloc:
    smio_set_exp_ops (self, NULL);
err_smio_set_exp_ops:
err_fill_desc:
    smio_set_thsafe_client_ops (self, NULL);
err_smio_set_thsafe_ops:
    smio_set_ops (self, NULL);
err_smio_set_ops:
err_set_name:
err_set_id:
    return err;
}

/* Destroy sm_io instance of swap */
smio_err_e swap_shutdown (smio_t *self)
{
    DBE_DEBUG (DBG_SM_IO | DBG_LVL_TRACE, "[sm_io:swap_exp] Shutting down swap\n");

    smio_err_e err = SMIO_SUCCESS;
    smio_swap_t *swap = smio_get_handler (self);
    ASSERT_TEST(swap != NULL, "Could not get SWAP handler",
            err_swap_handler, SMIO_ERR_ALLOC /* FIXME: improve return code */);

    /* Destroy SMIO instance */
    smio_swap_destroy (&swap);
    /* Nullify operation pointers */
    smio_set_exp_ops (self, NULL);
    smio_set_thsafe_client_ops (self, NULL);
    smio_set_ops (self, NULL);

err_swap_handler:
    return err;
}

const smio_bootstrap_ops_t swap_bootstrap_ops = {
    .init = swap_init,
    .shutdown = swap_shutdown,
    .config_defaults = swap_config_defaults
};

SMIO_MOD_DECLARE(SWAP_SDB_DEVID, SWAP_SDB_NAME, swap_bootstrap_ops)
