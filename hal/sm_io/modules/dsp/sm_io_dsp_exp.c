/*
 * Copyright (C) 2014 LNLS (www.lnls.br)
 * Author: Lucas Russo <lucas.russo@lnls.br>
 *
 * Released according to the GNU LGPL, version 3 or any later version.
 */

#include <stdlib.h>

#include "sm_io_dsp_exp.h"
#include "sm_io_dsp_codes.h"
#include "sm_io.h"
#include "dev_io.h"
#include "hal_assert.h"
#include "board.h"
#include "wb_pos_calc_regs.h"
#include "rw_param.h"
#include "rw_param_codes.h"
#include "sm_io_dsp_defaults.h"

/* Undef ASSERT_ALLOC to avoid conflicting with other ASSERT_ALLOC */
#ifdef ASSERT_TEST
#undef ASSERT_TEST
#endif
#define ASSERT_TEST(test_boolean, err_str, err_goto_label, /* err_core */ ...) \
    ASSERT_HAL_TEST(test_boolean, SM_IO, "[sm_io:dsp_exp]", \
            err_str, err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef ASSERT_ALLOC
#undef ASSERT_ALLOC
#endif
#define ASSERT_ALLOC(ptr, err_goto_label, /* err_core */ ...) \
    ASSERT_HAL_ALLOC(ptr, SM_IO, "[sm_io:dsp_exp]",         \
            smio_err_str(SMIO_ERR_ALLOC),                   \
            err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef CHECK_ERR
#undef CHECK_ERR
#endif
#define CHECK_ERR(err, err_type)                            \
    CHECK_HAL_ERR(err, SM_IO, "[sm_io:dsp_exp]",            \
            smio_err_str (err_type))

/************************************************************/
/*****************  Specific DSP Operations *****************/
/************************************************************/

#define KX_PARAM_MIN                        1
#define KX_PARAM_MAX                        ((1<<25)-1)
RW_PARAM_FUNC(dsp, kx) {
	SET_GET_PARAM(dsp, DSP_CTRL_REGS_OFFS, POS_CALC, KX, VAL, MULT_BIT_PARAM,
            KX_PARAM_MIN, KX_PARAM_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define KY_PARAM_MIN                        1
#define KY_PARAM_MAX                        ((1<<25)-1)
RW_PARAM_FUNC(dsp, ky) {
	SET_GET_PARAM(dsp, DSP_CTRL_REGS_OFFS, POS_CALC, KY, VAL, MULT_BIT_PARAM,
            KY_PARAM_MIN, KY_PARAM_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define KSUM_PARAM_MIN                      1
#define KSUM_PARAM_MAX                      ((1<<25)-1)
RW_PARAM_FUNC(dsp, ksum) {
	SET_GET_PARAM(dsp, DSP_CTRL_REGS_OFFS, POS_CALC, KSUM, VAL, MULT_BIT_PARAM,
            KSUM_PARAM_MIN, KSUM_PARAM_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define DS_TBT_THRES_MIN                    0
#define DS_TBT_THRES_MAX                    ((1<<26)-1)
RW_PARAM_FUNC(dsp, ds_tbt_thres) {
	SET_GET_PARAM(dsp, DSP_CTRL_REGS_OFFS, POS_CALC, DS_TBT_THRES, VAL, MULT_BIT_PARAM,
            DS_TBT_THRES_MIN, DS_TBT_THRES_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define DS_FOFB_THRES_MIN                   0
#define DS_FOFB_THRES_MAX                   ((1<<26)-1)
RW_PARAM_FUNC(dsp, ds_fofb_thres) {
	SET_GET_PARAM(dsp, DSP_CTRL_REGS_OFFS, POS_CALC, DS_FOFB_THRES, VAL, MULT_BIT_PARAM,
            DS_FOFB_THRES_MIN, DS_FOFB_THRES_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

#define DS_MONIT_THRES_MIN                  0
#define DS_MONIT_THRES_MAX                  ((1<<26)-1)
RW_PARAM_FUNC(dsp, ds_monit_thres) {
	SET_GET_PARAM(dsp, DSP_CTRL_REGS_OFFS, POS_CALC, DS_MONIT_THRES, VAL, MULT_BIT_PARAM,
            DS_MONIT_THRES_MIN, DS_MONIT_THRES_MAX, NO_CHK_FUNC, NO_FMT_FUNC, SET_FIELD);
}

/*
 * TODO: implement DDS set frequency
 * RW_PARAM_FUNC(dsp, dds_freq)
*/

#if 0
static void *_dsp_set_get_sw_on (void *owner, void *args) {
static void *_dsp_set_get_sw_off (void *owner, void *args) {
static void *_dsp_set_get_sw_clk_en_on (void *owner, void *args) {
static void *_dsp_set_get_sw_clk_en_off (void *owner, void *args) {
static void *_dsp_set_get_sw_divclk (void *owner, void *args) {
static void *_dsp_set_get_sw_phase (void *owner, void *args) {
static void *_dsp_set_get_wdw_on (void *owner, void *args) {
static void *_dsp_set_get_wdw_off (void *owner, void *args) {
static void *_dsp_set_get_wdw_dly (void *owner, void *args) {
static void *_dsp_set_get_adc_clk (void *owner, void *args) {
#endif

const smio_exp_ops_t dsp_exp_ops [] = {
    {.name 			= DSP_NAME_SET_GET_KX,
	 .opcode 		= DSP_OPCODE_SET_GET_KX,
	 .func_fp 		= RW_PARAM_FUNC_NAME(dsp, kx)				},

    {.name 			= DSP_NAME_SET_GET_KY,
	 .opcode 		= DSP_OPCODE_SET_GET_KY,
	 .func_fp 		= RW_PARAM_FUNC_NAME(dsp, ky)				},

    {.name 			= DSP_NAME_SET_GET_KSUM,
	 .opcode 		= DSP_OPCODE_SET_GET_KSUM,
	 .func_fp 		= RW_PARAM_FUNC_NAME(dsp, ksum) 			},

    {.name 			= DSP_NAME_SET_GET_DS_TBT_THRES,
	 .opcode 		= DSP_OPCODE_SET_GET_DS_TBT_THRES,
	 .func_fp 		= RW_PARAM_FUNC_NAME(dsp, ds_tbt_thres)     },

    {.name 			= DSP_NAME_SET_GET_DS_FOFB_THRES,
	 .opcode 		= DSP_OPCODE_SET_GET_DS_FOFB_THRES,
	 .func_fp 		= RW_PARAM_FUNC_NAME(dsp, ds_fofb_thres)    },

    {.name 			= DSP_NAME_SET_GET_DS_MONIT_THRES,
	 .opcode 		= DSP_OPCODE_SET_GET_DS_MONIT_THRES,
	 .func_fp 		= RW_PARAM_FUNC_NAME(dsp, ds_monit_thres)   },
/*
    {.name 			= DSP_NAME_SET_GET_DDS_FREQ,
	 .opcode 		= DSP_OPCODE_SET_GET_DDS_FREQ,
	 .func_fp 		= RW_PARAM_FUNC_NAME(DDS_FREQ)				},
*/

    {.name 			= NULL,		/* Must end with this NULL pattern */
	 .opcode 		= 0,
	 .func_fp 		= NULL										}
};
/************************************************************/
/***************** Export methods functions *****************/
/************************************************************/

static smio_err_e _dsp_do_op (void *owner, void *msg);

/* Attach an instance of sm_io to dev_io function pointer */
smio_err_e dsp_attach (smio_t *self, devio_t *parent)
{
    (void) self;
    (void) parent;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Deattach an instance of sm_io to dev_io function pointer */
smio_err_e dsp_deattach (smio_t *self)
{
    (void) self;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Export (register) sm_io to handle operations function pointer */
smio_err_e dsp_export_ops (smio_t *self,
        const smio_exp_ops_t* smio_exp_ops)
{
    (void) self;
    (void) smio_exp_ops;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Unexport (unregister) sm_io to handle operations function pointer */
smio_err_e dsp_unexport_ops (smio_t *self)
{
    (void) self;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

/* Generic wrapper for receiving opcodes and arguments to specific funtions function pointer */
/* FIXME: Code repetition! _devio_do_smio_op () function does almost the same!!! */
smio_err_e _dsp_do_op (void *owner, void *msg)
{
    (void) owner;
    (void) msg;
    return SMIO_ERR_FUNC_NOT_IMPL;
}

smio_err_e dsp_do_op (void *self, void *msg)
{
    return _dsp_do_op (self, msg);
}

const smio_ops_t dsp_ops = {
    .attach             = dsp_attach,          /* Attach sm_io instance to dev_io */
    .deattach           = dsp_deattach,        /* Deattach sm_io instance to dev_io */
    .export_ops         = dsp_export_ops,      /* Export sm_io operations to dev_io */
    .unexport_ops       = dsp_unexport_ops,    /* Unexport sm_io operations to dev_io */
    .do_op              = dsp_do_op            /* Generic wrapper for handling specific operations */
};

/************************************************************/
/****************** Bootstrap Operations ********************/
/************************************************************/

smio_err_e dsp_init (smio_t * self)
{
    DBE_DEBUG (DBG_SM_IO | DBG_LVL_TRACE, "[sm_io:dsp_exp] Initializing dsp\n");

    smio_err_e err = SMIO_SUCCESS;

    self->id = DSP_SDB_DEVID;
    self->name = strdup (DSP_SDB_NAME);
    ASSERT_ALLOC(self->name, err_name_alloc, SMIO_SUCCESS);

    /* Set SMIO ops pointers */
    self->ops = &dsp_ops;
    self->thsafe_client_ops = &smio_thsafe_client_zmq_ops;
    self->exp_ops = dsp_exp_ops;

    /* Initialize specific structure */
    self->smio_handler = smio_dsp_new (); /* TODO Define dsp init parameters */
    ASSERT_ALLOC(self->smio_handler, err_smio_handler_alloc, SMIO_SUCCESS);

//	_smio_dsp_config_defaults (self);

    return err;

err_smio_handler_alloc:
    free (self->name);
err_name_alloc:
    return err;
}

/* Destroy sm_io instance of dsp */
smio_err_e dsp_shutdown (smio_t *self)
{
    DBE_DEBUG (DBG_SM_IO | DBG_LVL_TRACE, "[sm_io:dsp_exp] Shutting down dsp\n");

    smio_dsp_destroy ((smio_dsp_t **)&self->smio_handler);
    self->exp_ops = NULL;
    self->thsafe_client_ops = NULL;
    self->ops = NULL;
    free (self->name);

    return SMIO_SUCCESS;
}

const smio_bootstrap_ops_t dsp_bootstrap_ops = {
    .init = dsp_init,
    .shutdown = dsp_shutdown,
    .config_defaults = dsp_config_defaults
};
