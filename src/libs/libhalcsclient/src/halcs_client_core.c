/*
 * Copyright (C) 2014 LNLS (www.lnls.br)
 * Author: Lucas Russo <lucas.russo@lnls.br>
 *
 * Released according to the GNU GPL, version 3 or any later version.
 */

#include "halcs_client.h"
/* Private headers */
#include "errhand.h"
#include "halcs_client_rw_param_codes.h"
#include "halcs_client_codes.h"
#include "sm_io_swap_useful_macros.h"
#include "halcs_client_revision.h"

/* Undef ASSERT_ALLOC to avoid conflicting with other ASSERT_ALLOC */
#ifdef ASSERT_TEST
#undef ASSERT_TEST
#endif
#define ASSERT_TEST(test_boolean, err_str, err_goto_label, /* err_core */ ...) \
    ASSERT_HAL_TEST(test_boolean, LIB_CLIENT, "[libclient]",\
            err_str, err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef ASSERT_ALLOC
#undef ASSERT_ALLOC
#endif
#define ASSERT_ALLOC(ptr, err_goto_label, /* err_core */ ...) \
    ASSERT_HAL_ALLOC(ptr, LIB_CLIENT, "[libclient]",        \
            halcs_client_err_str(HALCS_CLIENT_ERR_ALLOC),       \
            err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef CHECK_ERR
#undef CHECK_ERR
#endif
#define CHECK_ERR(err, err_type)                            \
    CHECK_HAL_ERR(err, LIB_CLIENT, "[libclient]",           \
            halcs_client_err_str (err_type))

#define HALCSCLIENT_DFLT_LOG_MODE             "w"
#define HALCSCLIENT_MLM_CONNECT_TIMEOUT       1000        /* in ms */
#define HALCSCLIENT_DFLT_TIMEOUT              1000        /* in ms */

/* Our structure */
struct _halcs_client_t {
    zuuid_t * uuid;                             /* Client UUID */
    mlm_client_t *mlm_client;                   /* Malamute client instance */
    int timeout;                                /* Timeout in msec for send/recv */
    zpoller_t *poller;                          /* Poller for receiving messages */
    const acq_chan_t *acq_chan;                 /* Acquisition buffer table */
};

static halcs_client_t *_halcs_client_new (char *broker_endp, int verbose,
        const char *log_file_name, const char *log_mode, int timeout);
static halcs_client_err_e _func_polling (halcs_client_t *self, char *name,
        char *service, uint32_t *input, uint32_t *output, int timeout);

/* Acquisition channel definitions for user's application */
#if defined(__BOARD_ML605__)
/* Global structure merging all of the channel's sample sizes */
acq_chan_t acq_chan[END_CHAN_ID] =  {   [0] = {.chan = ADC_CHAN_ID, .sample_size = ADC_SAMPLE_SIZE},
                                        [1] = {.chan = TBTAMP_CHAN_ID, .sample_size = TBTAMP_SAMPLE_SIZE},
                                        [2] = {.chan = TBTPOS_CHAN_ID, .sample_size = TBTPOS_SAMPLE_SIZE},
                                        [3] = {.chan = FOFBAMP_CHAN_ID, .sample_size = FOFBAMP_SAMPLE_SIZE},
                                        [4] = {.chan = FOFBPOS_CHAN_ID, .sample_size = FOFBPOS_SAMPLE_SIZE},
                                        [5] = {.chan = MONITAMP_CHAN_ID, .sample_size = MONITAMP_SAMPLE_SIZE},
                                        [6] = {.chan = MONITPOS_CHAN_ID, .sample_size = MONITPOS_SAMPLE_SIZE},
                                        [7] = {.chan = MONIT1POS_CHAN_ID, .sample_size = MONIT1POS_SAMPLE_SIZE}
                                    };
#elif defined(__BOARD_AFCV3__)
acq_chan_t acq_chan[END_CHAN_ID] =  {   [0]   =  {.chan = ADC_CHAN_ID, .sample_size = ADC_SAMPLE_SIZE},
                                        [1]   =  {.chan = ADCSWAP_CHAN_ID, .sample_size = ADCSWAP_SAMPLE_SIZE},
                                        [2]   =  {.chan = MIXIQ_CHAN_ID, .sample_size = MIXIQ_SAMPLE_SIZE},
                                        [3]   =  {.chan = DUMMY0_CHAN_ID, .sample_size = DUMMY0_SAMPLE_SIZE},
                                        [4]   =  {.chan = TBTDECIMIQ_CHAN_ID, .sample_size = TBTDECIMIQ_SAMPLE_SIZE},
                                        [5]   =  {.chan = DUMMY1_CHAN_ID, .sample_size = DUMMY1_SAMPLE_SIZE},
                                        [6]   =  {.chan = TBTAMP_CHAN_ID, .sample_size = TBTAMP_SAMPLE_SIZE},
                                        [7]   =  {.chan = TBTPHA_CHAN_ID, .sample_size = TBTPHA_SAMPLE_SIZE},
                                        [8]   =  {.chan = TBTPOS_CHAN_ID, .sample_size = TBTPOS_SAMPLE_SIZE},
                                        [9]   =  {.chan = FOFBDECIMIQ_CHAN_ID, .sample_size = FOFBDECIMIQ_SAMPLE_SIZE},
                                        [10]  =  {.chan = DUMMY2_CHAN_ID, .sample_size = DUMMY2_SAMPLE_SIZE},
                                        [11]  =  {.chan = FOFBAMP_CHAN_ID, .sample_size = FOFBAMP_SAMPLE_SIZE},
                                        [12]  =  {.chan = FOFBPHA_CHAN_ID, .sample_size = FOFBPHA_SAMPLE_SIZE},
                                        [13]  =  {.chan = FOFBPOS_CHAN_ID, .sample_size = FOFBPOS_SAMPLE_SIZE},
                                        [14]  =  {.chan = MONITAMP_CHAN_ID, .sample_size = MONITAMP_SAMPLE_SIZE},
                                        [15]  =  {.chan = MONITPOS_CHAN_ID, .sample_size = MONITPOS_SAMPLE_SIZE},
                                        [16]  =  {.chan = MONIT1POS_CHAN_ID, .sample_size = MONIT1POS_SAMPLE_SIZE}
                                    };
#else
#error "Unsupported board!"
#endif


/********************************************************/
/************************ Our API ***********************/
/********************************************************/
halcs_client_t *halcs_client_new (char *broker_endp, int verbose,
        const char *log_file_name)
{
    return _halcs_client_new (broker_endp, verbose, log_file_name,
            HALCSCLIENT_DFLT_LOG_MODE, HALCSCLIENT_DFLT_TIMEOUT);
}

halcs_client_t *halcs_client_new_log_mode (char *broker_endp, int verbose,
        const char *log_file_name, const char *log_mode)
{
    return _halcs_client_new (broker_endp, verbose, log_file_name,
            log_mode, HALCSCLIENT_DFLT_TIMEOUT);
}

halcs_client_t *halcs_client_new_log_mode_time (char *broker_endp, int verbose,
        const char *log_file_name, const char *log_mode, int timeout)
{
    return _halcs_client_new (broker_endp, verbose, log_file_name,
            log_mode, timeout);
}

halcs_client_t *halcs_client_new_time (char *broker_endp, int verbose,
        const char *log_file_name, int timeout)
{
    return _halcs_client_new (broker_endp, verbose, log_file_name,
            HALCSCLIENT_DFLT_LOG_MODE, timeout);
}

void halcs_client_destroy (halcs_client_t **self_p)
{
    assert (self_p);

    if (*self_p) {
        halcs_client_t *self = *self_p;

        self->acq_chan = NULL;
        zpoller_destroy (&self->poller);
        mlm_client_destroy (&self->mlm_client);
        zuuid_destroy (&self->uuid);
        free (self);
        *self_p = NULL;
    }
}

/*************************** Acessor methods *****************************/

zpoller_t *halcs_client_get_poller (halcs_client_t *self)
{
    return self->poller;
}

halcs_client_err_e halcs_client_set_timeout (halcs_client_t *self, int timeout)
{
    halcs_client_err_e err = HALCS_CLIENT_SUCCESS;
    self->timeout = timeout;
    return err;
}

uint32_t halcs_client_get_timeout (halcs_client_t *self)
{
    return self->timeout;
}

/**************** Static LIB Client Functions ****************/
static halcs_client_t *_halcs_client_new (char *broker_endp, int verbose,
        const char *log_file_name, const char *log_mode, int timeout)
{
    (void) verbose;

    assert (broker_endp);

    /* Set logfile available for all dev_mngr and dev_io instances.
     * We accept NULL as a parameter, meaning to suppress all messages */
    errhand_set_log (log_file_name, log_mode);

    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_INFO, "[libclient] Spawing LIBHALCSCLIENT"
            " with broker address %s, with logfile on %s ...\n", broker_endp,
            (log_file_name == NULL) ? "NULL" : log_file_name);

    /* Print Software info */
    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_INFO, "[libclient] HALCS Client version %s,"
            " Build by: %s, %s\n",
            revision_get_build_version (),
            revision_get_build_user_name (),
            revision_get_build_date ());

    halcs_client_t *self = zmalloc (sizeof *self);
    ASSERT_ALLOC(self, err_self_alloc);

    /* Generate UUID to work with MLM broker */
    self->uuid = zuuid_new ();
    ASSERT_ALLOC(self->uuid, err_uuid_alloc);

    self->mlm_client = mlm_client_new ();
    ASSERT_TEST(self->mlm_client!=NULL, "Could not create MLM client",
            err_mlm_client);

    /* Connect to broker with current UUID address in canonical form */
    int rc = mlm_client_connect (self->mlm_client, broker_endp,
            HALCSCLIENT_MLM_CONNECT_TIMEOUT, zuuid_str_canonical (self->uuid));
    ASSERT_TEST(rc >= 0, "Could not connect MLM client to broker", err_mlm_connect);

    /* Get MLM socket for use with poller */
    zsock_t *msgpipe = mlm_client_msgpipe (self->mlm_client);
    ASSERT_TEST (msgpipe != NULL, "Invalid MLM client socket reference",
            err_mlm_inv_client_socket);
    /* Initialize poller */
    self->poller = zpoller_new (msgpipe, NULL);
    ASSERT_TEST (self->poller != NULL, "Could not Initialize poller",
            err_init_poller);

    /* Initialize acquisition table */
    self->acq_chan = acq_chan;
    /* Initialize timeout */
    self->timeout = timeout;

    return self;

err_init_poller:
err_mlm_inv_client_socket:
err_mlm_connect:
    mlm_client_destroy (&self->mlm_client);
err_mlm_client:
    zuuid_destroy (&self->uuid);
err_uuid_alloc:
    free (self);
err_self_alloc:
    return NULL;
}

/**************** General Function to call the others *********/

halcs_client_err_e halcs_func_exec (halcs_client_t *self, const disp_op_t *func,
        char *service, uint32_t *input, uint32_t *output)
{
    halcs_client_err_e err = HALCS_CLIENT_SUCCESS;
    uint8_t *input8 = (uint8_t *) input;
    uint8_t *output8 = (uint8_t *) output;

    /* Check input arguments */
    ASSERT_TEST(self != NULL, "Bpm_client is NULL", err_null_exp,
            HALCS_CLIENT_ERR_INV_FUNCTION);
    ASSERT_TEST(func != NULL, "Function structure is NULL", err_null_exp,
            HALCS_CLIENT_ERR_INV_FUNCTION);
    ASSERT_TEST(!(func->args[0] != DISP_ARG_END && input8 == NULL),
            "Invalid input arguments!", err_inv_param, HALCS_CLIENT_ERR_INV_PARAM);
    ASSERT_TEST(!(func->retval != DISP_ARG_END && output8 == NULL),
            "Invalid output arguments!", err_inv_param, HALCS_CLIENT_ERR_INV_PARAM);

    /* Create the message */
    zmsg_t *msg = zmsg_new ();
    ASSERT_ALLOC(msg, err_msg_alloc, HALCS_CLIENT_ERR_ALLOC);

    /* Add the frame containing the opcode for the function desired (always first) */
    zmsg_addmem (msg, &func->opcode, sizeof (func->opcode));

    /* Add the arguments in their respective frames in the message */
    for (int i = 0; func->args[i] != DISP_ARG_END; ++i) {
        /* Get the size of the message being sent */
        uint32_t in_size = DISP_GET_ASIZE(func->args[i]);
        /* Create a frame to compose the message */
        zmsg_addmem (msg, input8, in_size);
        /* Moves along the pointer */
        input8 += in_size;
    }

    mlm_client_sendto (self->mlm_client, service, NULL, NULL, 0, &msg);

    /* Receive report */
    zmsg_t *report = param_client_recv_timeout (self);
    ASSERT_TEST(report != NULL, "Report received is NULL", err_msg);

    /* Message is:
     * frame 0: Error code
     * frame 1: Number of bytes received
     * frame 2+: Data received      */

    /* Handling malformed messages */
    size_t msg_size = zmsg_size (report);
    ASSERT_TEST(msg_size == MSG_ERR_CODE_SIZE || msg_size == MSG_FULL_SIZE,
            "Unexpected message received", err_msg);

    /* Get message contents */
    zframe_t *err_code = zmsg_pop(report);
    ASSERT_TEST(err_code != NULL, "Could not receive error code", err_msg);
    err = *(uint32_t *)zframe_data(err_code);

    zframe_t *data_size_frm = NULL;
    zframe_t *data_frm = NULL;
    if (msg_size == MSG_FULL_SIZE)
    {
        data_size_frm = zmsg_pop (report);
        ASSERT_TEST(data_size_frm != NULL, "Could not receive data size", err_null_data_size);
        data_frm = zmsg_pop (report);
        ASSERT_TEST(data_frm != NULL, "Could not receive data", err_null_data);

        ASSERT_TEST(zframe_size (data_size_frm) == RW_REPLY_SIZE,
                "Wrong <number of payload bytes> parameter size", err_msg_fmt);

        /* Size in the second frame must match the frame size of the third */
        RW_REPLY_TYPE data_size = *(RW_REPLY_TYPE *) zframe_data(data_size_frm);
        ASSERT_TEST(data_size == zframe_size (data_frm),
                "<payload> parameter size does not match size in <number of payload bytes> parameter",
                err_msg_fmt);

        uint32_t *data_out = (uint32_t *)zframe_data(data_frm);
        /* Copy message contents to user */
        memcpy (output8, data_out, data_size);
    }

err_msg_fmt:
    zframe_destroy (&data_frm);
err_null_data:
    zframe_destroy (&data_size_frm);
err_null_data_size:
    zframe_destroy (&err_code);
err_msg:
    zmsg_destroy (&report);
err_msg_alloc:
    zmsg_destroy (&msg);
err_null_exp:
err_inv_param:
    return err;
}

const disp_op_t *halcs_func_translate (char *name)
{
    assert (name);

    /* Search the function table for a match in the 'name' field */
    for (int i=0; smio_exp_ops[i] != NULL; i++) {
        for (int j=0; smio_exp_ops[i][j] != NULL; j++) {
            if (streq(name, smio_exp_ops[i][j]->name)) {
                return smio_exp_ops[i][j];
            }
        }
    }

    return 0;
}

halcs_client_err_e halcs_func_trans_exec (halcs_client_t *self, char *name, char *service, uint32_t *input, uint32_t *output)
{
    const disp_op_t *func = halcs_func_translate(name);
    halcs_client_err_e err = halcs_func_exec (self, func, service, input, output);
    return err;
}

/********************** Accessor Methods **********************/

mlm_client_t *halcs_get_mlm_client (halcs_client_t *self)
{
    assert (self);
    return self->mlm_client;
}

/**************** FMC ADC COMMON SMIO Functions ****************/

PARAM_FUNC_CLIENT_WRITE(fmc_leds)
{
    return param_client_write (self, service, FMC_ADC_COMMON_OPCODE_LEDS,
            fmc_leds);
}

PARAM_FUNC_CLIENT_READ(fmc_leds)
{
    return param_client_read (self, service, FMC_ADC_COMMON_OPCODE_LEDS,
            fmc_leds);
}

PARAM_FUNC_CLIENT_WRITE(adc_test_data_en)
{
    return param_client_write (self, service, FMC_ADC_COMMON_OPCODE_TEST_DATA_EN, adc_test_data_en);
}

PARAM_FUNC_CLIENT_READ(adc_test_data_en)
{
     return param_client_read (self, service, FMC_ADC_COMMON_OPCODE_TEST_DATA_EN, adc_test_data_en);
}

PARAM_FUNC_CLIENT_WRITE(trig_dir)
{
    return param_client_write (self, service, FMC_ADC_COMMON_OPCODE_TRIG_DIR, trig_dir);
}

PARAM_FUNC_CLIENT_READ(trig_dir)
{
    return param_client_read (self, service, FMC_ADC_COMMON_OPCODE_TRIG_DIR, trig_dir);
}

PARAM_FUNC_CLIENT_WRITE(trig_term)
{
    return param_client_write (self, service, FMC_ADC_COMMON_OPCODE_TRIG_TERM, trig_term);
}

PARAM_FUNC_CLIENT_READ(trig_term)
{
    return param_client_read (self, service, FMC_ADC_COMMON_OPCODE_TRIG_TERM, trig_term);
}

PARAM_FUNC_CLIENT_WRITE(trig_val)
{
    return param_client_write (self, service, FMC_ADC_COMMON_OPCODE_TRIG_VAL, trig_val);
}

PARAM_FUNC_CLIENT_READ(trig_val)
{
    return param_client_read (self, service, FMC_ADC_COMMON_OPCODE_TRIG_VAL, trig_val);
}

/**************** FMC ACTIVE CLOCK SMIO Functions ****************/

/* FUNCTION pin functions */
PARAM_FUNC_CLIENT_WRITE(fmc_pll_function)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_PLL_FUNCTION,
            fmc_pll_function);
}

PARAM_FUNC_CLIENT_READ(fmc_pll_function)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_PLL_FUNCTION,
            fmc_pll_function);
}

/* STATUS pin functions */
PARAM_FUNC_CLIENT_WRITE(fmc_pll_status)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_PLL_STATUS,
            fmc_pll_status);
}

PARAM_FUNC_CLIENT_READ(fmc_pll_status)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_PLL_STATUS,
            fmc_pll_status);
}

/* CLK_SEL pin functions */
PARAM_FUNC_CLIENT_WRITE(fmc_clk_sel)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_CLK_SEL,
            fmc_clk_sel);
}

PARAM_FUNC_CLIENT_READ(fmc_clk_sel)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_CLK_SEL,
            fmc_clk_sel);
}

PARAM_FUNC_CLIENT_WRITE(si571_oe)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_SI571_OE, si571_oe);
}

PARAM_FUNC_CLIENT_READ(si571_oe)
{
     return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_SI571_OE, si571_oe);
}

PARAM_FUNC_CLIENT_WRITE(ad9510_defaults)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_CFG_DEFAULTS,
            ad9510_defaults);
}

PARAM_FUNC_CLIENT_READ(ad9510_defaults)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_CFG_DEFAULTS,
            ad9510_defaults);
}

/* AD9510 PLL A divider */
PARAM_FUNC_CLIENT_WRITE(ad9510_pll_a_div)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_A_DIV,
            ad9510_pll_a_div);
}

PARAM_FUNC_CLIENT_READ(ad9510_pll_a_div)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_A_DIV,
            ad9510_pll_a_div);
}

/* AD9510 PLL B divider */
PARAM_FUNC_CLIENT_WRITE(ad9510_pll_b_div)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_B_DIV,
            ad9510_pll_b_div);
}

PARAM_FUNC_CLIENT_READ(ad9510_pll_b_div)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_B_DIV,
            ad9510_pll_b_div);
}

/* AD9510 PLL Prescaler */
PARAM_FUNC_CLIENT_WRITE(ad9510_pll_prescaler)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_PRESCALER,
            ad9510_pll_prescaler);
}

PARAM_FUNC_CLIENT_READ(ad9510_pll_prescaler)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_PRESCALER,
            ad9510_pll_prescaler);
}

/* AD9510 R divider */
PARAM_FUNC_CLIENT_WRITE(ad9510_r_div)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_R_DIV,
            ad9510_r_div);
}

PARAM_FUNC_CLIENT_READ(ad9510_r_div)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_R_DIV,
            ad9510_r_div);
}

/* AD9510 PLL Power Down */
PARAM_FUNC_CLIENT_WRITE(ad9510_pll_pdown)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_PDOWN,
            ad9510_pll_pdown);
}

PARAM_FUNC_CLIENT_READ(ad9510_pll_pdown)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_PDOWN,
            ad9510_pll_pdown);
}

/* AD9510 Mux Status */
PARAM_FUNC_CLIENT_WRITE(ad9510_mux_status)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_MUX_STATUS,
            ad9510_mux_status);
}

PARAM_FUNC_CLIENT_READ(ad9510_mux_status)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_MUX_STATUS,
            ad9510_mux_status);
}

/* AD9510 CP current */
PARAM_FUNC_CLIENT_WRITE(ad9510_cp_current)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_CP_CURRENT,
            ad9510_cp_current);
}

PARAM_FUNC_CLIENT_READ(ad9510_cp_current)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_CP_CURRENT,
            ad9510_cp_current);
}

/* AD9510 Outputs */
PARAM_FUNC_CLIENT_WRITE(ad9510_outputs)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_OUTPUTS,
            ad9510_outputs);
}

PARAM_FUNC_CLIENT_READ(ad9510_outputs)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_OUTPUTS,
            ad9510_outputs);
}

/* AD9510 PLL CLK Selection */
PARAM_FUNC_CLIENT_WRITE(ad9510_pll_clk_sel)
{
    return param_client_write (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_CLK_SEL,
            ad9510_pll_clk_sel);
}

PARAM_FUNC_CLIENT_READ(ad9510_pll_clk_sel)
{
    return param_client_read (self, service, FMC_ACTIVE_CLK_OPCODE_AD9510_PLL_CLK_SEL,
            ad9510_pll_clk_sel);
}

/* SI571 Set frequency */
PARAM_FUNC_CLIENT_WRITE_DOUBLE(si571_freq)
{
    return param_client_write_double (self, service, FMC_ACTIVE_CLK_OPCODE_SI571_FREQ,
            si571_freq);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(si571_freq)
{
    return param_client_read_double (self, service, FMC_ACTIVE_CLK_OPCODE_SI571_FREQ,
            si571_freq);
}

/* SI571 Get defaults */
PARAM_FUNC_CLIENT_WRITE_DOUBLE(si571_defaults)
{
    return param_client_write_double (self, service, FMC_ACTIVE_CLK_OPCODE_SI571_GET_DEFAULTS,
            si571_defaults);
}

/**************** FMC 130M SMIO Functions ****************/

/* ADC LTC2208 RAND */
PARAM_FUNC_CLIENT_WRITE(adc_rand)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_RAND, adc_rand);
}

PARAM_FUNC_CLIENT_READ(adc_rand)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_RAND, adc_rand);
}

/* ADC LTC2208 DITH */
PARAM_FUNC_CLIENT_WRITE(adc_dith)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DITH, adc_dith);
}

PARAM_FUNC_CLIENT_READ(adc_dith)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DITH, adc_dith);
}

/* ADC LTC2208 SHDN */
PARAM_FUNC_CLIENT_WRITE(adc_shdn)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_SHDN, adc_shdn);
}

PARAM_FUNC_CLIENT_READ(adc_shdn)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_SHDN, adc_shdn);
}

/* ADC LTC2208 PGA */
PARAM_FUNC_CLIENT_WRITE(adc_pga)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_PGA, adc_pga);
}

PARAM_FUNC_CLIENT_READ(adc_pga)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_PGA, adc_pga);
}

/* RAW ADC data 0 value */
PARAM_FUNC_CLIENT_WRITE(adc_data0)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DATA0, adc_data0);
}

PARAM_FUNC_CLIENT_READ(adc_data0)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DATA0, adc_data0);
}

/* RAW ADC data 1 value */
PARAM_FUNC_CLIENT_WRITE(adc_data1)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DATA1, adc_data1);
}

PARAM_FUNC_CLIENT_READ(adc_data1)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DATA1, adc_data1);
}

/* RAW ADC data 2 value */
PARAM_FUNC_CLIENT_WRITE(adc_data2)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DATA2, adc_data2);
}

PARAM_FUNC_CLIENT_READ(adc_data2)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DATA2, adc_data2);
}

/* RAW ADC data 3 value */
PARAM_FUNC_CLIENT_WRITE(adc_data3)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DATA3, adc_data3);
}

PARAM_FUNC_CLIENT_READ(adc_data3)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DATA3, adc_data3);
}

/****************** FMC130M Delay Value Functions ****************/

/* ADC delay value 0 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_val0)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL0, adc_dly_val0);
}

PARAM_FUNC_CLIENT_READ(adc_dly_val0)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL0, adc_dly_val0);
}

/* ADC delay value 1 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_val1)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL1, adc_dly_val1);
}

PARAM_FUNC_CLIENT_READ(adc_dly_val1)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL1, adc_dly_val1);
}

/* ADC delay value 2 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_val2)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL2, adc_dly_val2);
}

PARAM_FUNC_CLIENT_READ(adc_dly_val2)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL2, adc_dly_val2);
}

/* ADC delay value 3 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_val3)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL3, adc_dly_val3);
}

PARAM_FUNC_CLIENT_READ(adc_dly_val3)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_VAL3, adc_dly_val3);
}

/****************** FMC130M Delay Line selection Functions ****************/

/* ADC line value 0 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_line0)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE0, adc_dly_line0);
}

PARAM_FUNC_CLIENT_READ(adc_dly_line0)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE0, adc_dly_line0);
}

/* ADC line value 1 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_line1)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE1, adc_dly_line1);
}

PARAM_FUNC_CLIENT_READ(adc_dly_line1)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE1, adc_dly_line1);
}

/* ADC line value 2 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_line2)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE2, adc_dly_line2);
}

PARAM_FUNC_CLIENT_READ(adc_dly_line2)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE2, adc_dly_line2);
}

/* ADC line value 3 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_line3)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE3, adc_dly_line3);
}

PARAM_FUNC_CLIENT_READ(adc_dly_line3)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_LINE3, adc_dly_line3);
}

/****************** FMC130M Delay update Functions ****************/

/* ADC Update channel 0 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_updt0)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT0, adc_dly_updt0);
}

PARAM_FUNC_CLIENT_READ(adc_dly_updt0)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT0, adc_dly_updt0);
}

/* ADC Update channel 1 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_updt1)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT1, adc_dly_updt1);
}

PARAM_FUNC_CLIENT_READ(adc_dly_updt1)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT1, adc_dly_updt1);
}

/* ADC Update channel 2 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_updt2)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT2, adc_dly_updt2);
}

PARAM_FUNC_CLIENT_READ(adc_dly_updt2)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT2, adc_dly_updt2);
}

/* ADC Update channel 3 */
PARAM_FUNC_CLIENT_WRITE(adc_dly_updt3)
{
    return param_client_write (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT3, adc_dly_updt3);
}

PARAM_FUNC_CLIENT_READ(adc_dly_updt3)
{
     return param_client_read (self, service, FMC130M_4CH_OPCODE_ADC_DLY_UPDT3, adc_dly_updt3);
}

/****************** FMC130M set delay functions ****************/

/* ADC set delay channel 0 */
PARAM_FUNC_CLIENT_WRITE2(adc_dly0, type, val)
{
    return param_client_write_raw (self, service, FMC130M_4CH_OPCODE_ADC_DLY0,
            type, val);
}

/* ADC set delay channel 1 */
PARAM_FUNC_CLIENT_WRITE2(adc_dly1, type, val)
{
    return param_client_write_raw (self, service, FMC130M_4CH_OPCODE_ADC_DLY1,
            type, val);
}

/* ADC set delay channel 2 */
PARAM_FUNC_CLIENT_WRITE2(adc_dly2, type, val)
{
    return param_client_write_raw (self, service, FMC130M_4CH_OPCODE_ADC_DLY2,
            type, val);
}
/* ADC set delay channel 3 */
PARAM_FUNC_CLIENT_WRITE2(adc_dly3, type, val)
{
    return param_client_write_raw (self, service, FMC130M_4CH_OPCODE_ADC_DLY3,
            type, val);
}

/*************************** FMC250M Chips Functions *************************/

/* ISLA216P RST ADCs */
PARAM_FUNC_CLIENT_WRITE(rst_adcs)
{
    return param_client_write (self, service, FMC250M_4CH_OPCODE_RST_ADCS,
            rst_adcs);
}

PARAM_FUNC_CLIENT_READ(rst_adcs)
{
    return param_client_read (self, service, FMC250M_4CH_OPCODE_RST_ADCS,
            rst_adcs);
}

/* ISLA216P RST ADCs */
PARAM_FUNC_CLIENT_WRITE(rst_div_adcs)
{
    return param_client_write (self, service, FMC250M_4CH_OPCODE_RST_DIV_ADCS,
            rst_div_adcs);
}

PARAM_FUNC_CLIENT_READ(rst_div_adcs)
{
    return param_client_read (self, service, FMC250M_4CH_OPCODE_RST_DIV_ADCS,
            rst_div_adcs);
}

/* ISLA216P Sleep ADCs */
PARAM_FUNC_CLIENT_WRITE(sleep_adcs)
{
    return param_client_write (self, service, FMC250M_4CH_OPCODE_SLEEP_ADCS,
            sleep_adcs);
}

PARAM_FUNC_CLIENT_READ(sleep_adcs)
{
    return param_client_read (self, service, FMC250M_4CH_OPCODE_SLEEP_ADCS,
            sleep_adcs);
}

/* ISLA216P Test modes */
PARAM_FUNC_CLIENT_WRITE(test_mode0)
{
    return param_client_write (self, service, FMC250M_4CH_OPCODE_TESTMODE0,
            test_mode0);
}

PARAM_FUNC_CLIENT_WRITE(test_mode1)
{
    return param_client_write (self, service, FMC250M_4CH_OPCODE_TESTMODE1,
            test_mode1);
}

PARAM_FUNC_CLIENT_WRITE(test_mode2)
{
    return param_client_write (self, service, FMC250M_4CH_OPCODE_TESTMODE2,
            test_mode2);
}

PARAM_FUNC_CLIENT_WRITE(test_mode3)
{
    return param_client_write (self, service, FMC250M_4CH_OPCODE_TESTMODE3,
            test_mode3);
}

/****************** ACQ SMIO Functions ****************/
#define MIN_WAIT_TIME           1                           /* in ms */
#define MSECS                   1000                        /* in seconds */

static halcs_client_err_e _halcs_acq_start (halcs_client_t *self, char *service,
        acq_req_t *acq_req);
static halcs_client_err_e _halcs_acq_check (halcs_client_t *self, char *service);
static halcs_client_err_e _halcs_acq_check_timed (halcs_client_t *self, char *service,
        int timeout);
static halcs_client_err_e _halcs_acq_get_data_block (halcs_client_t *self,
        char *service, acq_trans_t *acq_trans);
static halcs_client_err_e _halcs_acq_get_curve (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans);
static halcs_client_err_e _halcs_full_acq (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans, int timeout);
static halcs_client_err_e _halcs_full_acq_compat (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans, int timeout, bool new_acq);

halcs_client_err_e halcs_acq_start (halcs_client_t *self, char *service, acq_req_t *acq_req)
{
    return _halcs_acq_start (self, service, acq_req);
}

halcs_client_err_e halcs_acq_check (halcs_client_t *self, char *service)
{
    return _halcs_acq_check (self, service);
}

halcs_client_err_e halcs_acq_check_timed (halcs_client_t *self, char *service,
        int timeout)
{
    return _halcs_acq_check_timed (self, service, timeout);
}

halcs_client_err_e halcs_acq_get_data_block (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans)
{
    return _halcs_acq_get_data_block (self, service, acq_trans);
}

halcs_client_err_e halcs_acq_get_curve (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans)
{
    return _halcs_acq_get_curve (self, service, acq_trans);
}

halcs_client_err_e halcs_full_acq (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans, int timeout)
{
    return _halcs_full_acq (self, service, acq_trans, timeout);
}

halcs_client_err_e halcs_full_acq_compat (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans, int timeout, bool new_acq)
{
    return _halcs_full_acq_compat (self, service, acq_trans, timeout, new_acq);
}

static halcs_client_err_e _halcs_acq_check_timed (halcs_client_t *self, char *service,
        int timeout)
{
    return func_polling (self, ACQ_NAME_CHECK_DATA_ACQUIRE, service, NULL,
            NULL, timeout);
}

halcs_client_err_e halcs_set_acq_trig (halcs_client_t *self, char *service,
        uint32_t trig)
{
    return param_client_write (self, service, ACQ_OPCODE_CFG_TRIG, trig);
}

halcs_client_err_e halcs_get_acq_trig (halcs_client_t *self, char *service,
        uint32_t *trig)
{
    return param_client_read (self, service, ACQ_OPCODE_CFG_TRIG, trig);
}

halcs_client_err_e halcs_set_acq_data_trig_pol (halcs_client_t *self, char *service,
        uint32_t data_trig_pol)
{
    return param_client_write (self, service, ACQ_OPCODE_HW_DATA_TRIG_POL,
            data_trig_pol);
}

halcs_client_err_e halcs_get_acq_data_trig_pol (halcs_client_t *self, char *service,
        uint32_t *data_trig_pol)
{
    return param_client_read (self, service, ACQ_OPCODE_HW_DATA_TRIG_POL,
            data_trig_pol);
}

halcs_client_err_e halcs_set_acq_data_trig_sel (halcs_client_t *self, char *service,
        uint32_t data_trig_sel)
{
    return param_client_write (self, service, ACQ_OPCODE_HW_DATA_TRIG_SEL,
            data_trig_sel);
}

halcs_client_err_e halcs_get_acq_data_trig_sel (halcs_client_t *self, char *service,
        uint32_t *data_trig_sel)
{
    return param_client_read (self, service, ACQ_OPCODE_HW_DATA_TRIG_SEL,
            data_trig_sel);
}

halcs_client_err_e halcs_set_acq_data_trig_filt (halcs_client_t *self, char *service,
        uint32_t data_trig_filt)
{
    return param_client_write (self, service, ACQ_OPCODE_HW_DATA_TRIG_FILT,
            data_trig_filt);
}
halcs_client_err_e halcs_get_acq_data_trig_filt (halcs_client_t *self, char *service,
        uint32_t *data_trig_filt)
{
    return param_client_read (self, service, ACQ_OPCODE_HW_DATA_TRIG_FILT,
            data_trig_filt);
}

halcs_client_err_e halcs_set_acq_data_trig_thres (halcs_client_t *self, char *service,
        uint32_t data_trig_thres)
{
    return param_client_write (self, service, ACQ_OPCODE_HW_DATA_TRIG_THRES,
            data_trig_thres);
}

halcs_client_err_e halcs_get_acq_data_trig_thres (halcs_client_t *self, char *service,
        uint32_t *data_trig_thres)
{
    return param_client_read (self, service, ACQ_OPCODE_HW_DATA_TRIG_THRES,
            data_trig_thres);
}

halcs_client_err_e halcs_set_acq_hw_trig_dly (halcs_client_t *self, char *service,
        uint32_t hw_trig_dly)
{
    return param_client_write (self, service, ACQ_OPCODE_HW_TRIG_DLY,
            hw_trig_dly);
}

halcs_client_err_e halcs_get_acq_hw_trig_dly (halcs_client_t *self, char *service,
        uint32_t *hw_trig_dly)
{
    return param_client_read (self, service, ACQ_OPCODE_HW_TRIG_DLY,
            hw_trig_dly);
}

halcs_client_err_e halcs_set_acq_sw_trig (halcs_client_t *self, char *service,
        uint32_t sw_trig)
{
    return param_client_write (self, service, ACQ_OPCODE_SW_TRIG,
            sw_trig);
}

halcs_client_err_e halcs_get_acq_sw_trig (halcs_client_t *self, char *service,
        uint32_t *sw_trig)
{
    return param_client_read (self, service, ACQ_OPCODE_SW_TRIG,
            sw_trig);
}

halcs_client_err_e halcs_set_acq_fsm_stop (halcs_client_t *self, char *service,
        uint32_t fsm_stop)
{
    return param_client_write (self, service, ACQ_OPCODE_FSM_STOP,
            fsm_stop);
}

halcs_client_err_e halcs_get_acq_fsm_stop (halcs_client_t *self, char *service,
        uint32_t *fsm_stop)
{
    return param_client_read (self, service, ACQ_OPCODE_FSM_STOP,
            fsm_stop);
}

halcs_client_err_e halcs_set_acq_data_trig_chan (halcs_client_t *self, char *service,
        uint32_t data_trig_chan)
{
    return param_client_write (self, service, ACQ_OPCODE_HW_DATA_TRIG_CHAN,
            data_trig_chan);
}

halcs_client_err_e halcs_get_acq_data_trig_chan (halcs_client_t *self, char *service,
        uint32_t *data_trig_chan)
{
    return param_client_read (self, service, ACQ_OPCODE_HW_DATA_TRIG_CHAN,
            data_trig_chan);
}

static halcs_client_err_e _halcs_acq_start (halcs_client_t *self, char *service, acq_req_t *acq_req)
{
    assert (self);
    assert (service);
    assert (acq_req);

    uint32_t write_val[4] = {0};
    write_val[0] = acq_req->num_samples_pre;
    write_val[1] = acq_req->num_samples_post;
    write_val[2] = acq_req->num_shots;
    write_val[3] = acq_req->chan;

    const disp_op_t* func = halcs_func_translate(ACQ_NAME_DATA_ACQUIRE);
    halcs_client_err_e err = halcs_func_exec(self, func, service, write_val, NULL);

    /* Check if any error occurred */
    ASSERT_TEST(err == HALCS_CLIENT_SUCCESS, "halcs_data_acquire: Data acquire was "
            "not requested correctly", err_data_acquire, HALCS_CLIENT_ERR_AGAIN);

    /* If we are here, then the request was successfully acquired*/
    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_data_acquire: "
            "Data acquire was successfully required\n");

err_data_acquire:
    return err;
}

static halcs_client_err_e _halcs_acq_check (halcs_client_t *self, char *service)
{
    assert (self);
    assert (service);

    const disp_op_t* func = halcs_func_translate(ACQ_NAME_CHECK_DATA_ACQUIRE);
    halcs_client_err_e err = halcs_func_exec(self, func, service, NULL, NULL);

    if (err != HALCS_CLIENT_SUCCESS) {
        DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] data acquisition "
                "was not completed\n");
    }
    else {
        DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] data acquisition "
                "was completed\n");
    }

    return err;
}

static halcs_client_err_e _halcs_acq_get_data_block (halcs_client_t *self, char *service, acq_trans_t *acq_trans)
{
    assert (self);
    assert (service);
    assert (acq_trans);
    assert (acq_trans->block.data);

    halcs_client_err_e err = HALCS_CLIENT_SUCCESS;

    uint32_t write_val[2] = {0};
    write_val[0] = acq_trans->req.chan;
    write_val[1] = acq_trans->block.idx;

    smio_acq_data_block_t read_val[1];

    /* Sent Message is:
     * frame 0: operation code
     * frame 1: channel
     * frame 2: block required */

    const disp_op_t* func = halcs_func_translate(ACQ_NAME_GET_DATA_BLOCK);
    err = halcs_func_exec(self, func, service, write_val, (uint32_t *) read_val);

    /* Message is:
     * frame 0: error code
     * frame 1: number of bytes read (optional)
     * frame 2: data read (optional) */

    /* Check if any error occurred */
    ASSERT_TEST(err == HALCS_CLIENT_SUCCESS,
            "halcs_get_data_block: Data block was not acquired",
            err_get_data_block, HALCS_CLIENT_ERR_SERVER);

    /* Data size effectively returned */
    uint32_t read_size = (acq_trans->block.data_size < read_val->valid_bytes) ?
        acq_trans->block.data_size : read_val->valid_bytes;

    /* Copy message contents to user */
    memcpy (acq_trans->block.data, read_val->data, read_size);

    /* Inform user about the number of bytes effectively copied */
    acq_trans->block.bytes_read = read_size;

    /* Print some debug messages */
    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_get_data_block: "
            "read_size: %u\n", read_size);
    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_get_data_block: "
            "acq_trans->block.data: %p\n", acq_trans->block.data);

err_get_data_block:
    return err;
}

static halcs_client_err_e _halcs_acq_get_curve (halcs_client_t *self, char *service, acq_trans_t *acq_trans)
{
    assert (self);
    assert (service);
    assert (acq_trans);
    assert (acq_trans->block.data);

    halcs_client_err_e err = HALCS_CLIENT_SUCCESS;

    uint32_t num_samples_shot = acq_trans->req.num_samples_pre +
        acq_trans->req.num_samples_post;
    uint32_t num_samples_multishot = num_samples_shot*acq_trans->req.num_shots;
    uint32_t n_max_samples = BLOCK_SIZE/self->acq_chan[acq_trans->req.chan].sample_size;
    uint32_t block_n_valid = num_samples_multishot / n_max_samples;
    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_get_curve: "
            "block_n_valid = %u\n", block_n_valid);

    /* Total bytes read */
    uint32_t total_bread = 0;
    /* Save the original buffer size for later */
    uint32_t data_size = acq_trans->block.data_size;
    uint32_t *original_data_pt = acq_trans->block.data;

    /* Fill all blocks */
    for (uint32_t block_n = 0; block_n <= block_n_valid; block_n++) {
        if (zsys_interrupted) {
            err = HALCS_CLIENT_INT;
            goto halcs_zsys_interrupted;
        }

        acq_trans->block.idx = block_n;
        err = _halcs_acq_get_data_block (self, service, acq_trans);

        /* Check for return code */
        ASSERT_TEST(err == HALCS_CLIENT_SUCCESS,
                "_halcs_get_data_block failed. block_n is probably out of range",
                err_halcs_get_data_block);

        total_bread += acq_trans->block.bytes_read;
        acq_trans->block.data = (uint32_t *)((uint8_t *)acq_trans->block.data + acq_trans->block.bytes_read);
        acq_trans->block.data_size -= acq_trans->block.bytes_read;

        /* Print some debug messages */
        DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_get_curve: "
                "Total bytes read up to now: %u\n", total_bread);
        DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_get_curve: "
                "Data pointer addr: %p\n", acq_trans->block.data);
        DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_get_curve: "
                "Data buffer size: %u\n", acq_trans->block.data_size);
    }

    /* Return to client the total number of bytes read */
    acq_trans->block.bytes_read = total_bread;
    acq_trans->block.data_size = data_size;
    acq_trans->block.data = original_data_pt;

    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] halcs_get_curve: "
            "Data curve of %u bytes was successfully acquired\n", total_bread);

halcs_zsys_interrupted:
err_halcs_get_data_block:
    return err;
}

static halcs_client_err_e _halcs_full_acq (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans, int timeout)
{
    assert (self);
    assert (service);
    assert (acq_trans);
    assert (acq_trans->block.data);

    /* Send Acquisition Request */
    _halcs_acq_start (self, service, &acq_trans->req);

    /* Wait until the acquisition is finished */
    halcs_client_err_e err = _func_polling (self, ACQ_NAME_CHECK_DATA_ACQUIRE,
            service, NULL, NULL, timeout);

    ASSERT_TEST(err == HALCS_CLIENT_SUCCESS,
            "Data acquisition was not completed",
            err_check_data_acquire, HALCS_CLIENT_ERR_SERVER);

    /* If we are here, then the acquisition was successfully completed*/
    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] "
            "Data acquisition was successfully completed\n");

    err = _halcs_acq_get_curve (self, service, acq_trans);
    ASSERT_TEST(err == HALCS_CLIENT_SUCCESS, "Could not get requested curve",
            err_get_curve, HALCS_CLIENT_ERR_SERVER);

err_get_curve:
err_check_data_acquire:
    return err;
}

/* Wrapper to be compatible with old function behavior */
static halcs_client_err_e _halcs_full_acq_compat (halcs_client_t *self, char *service,
        acq_trans_t *acq_trans, int timeout, bool new_acq)
{
    if (new_acq) {
        return _halcs_full_acq (self, service, acq_trans, timeout);
    }
    else {
        return _halcs_acq_get_curve (self, service, acq_trans);
    }
}

/**************** DSP SMIO Functions ****************/

/* Kx functions */
PARAM_FUNC_CLIENT_WRITE(kx)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_KX, kx);
}

PARAM_FUNC_CLIENT_READ(kx)
{
    return param_client_read (self, service, DSP_OPCODE_SET_GET_KX, kx);
}

/* Ky functions */
PARAM_FUNC_CLIENT_WRITE(ky)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_KY, ky);
}

PARAM_FUNC_CLIENT_READ(ky)
{
    return param_client_read (self, service, DSP_OPCODE_SET_GET_KY, ky);
}

/* Ksum functions */
PARAM_FUNC_CLIENT_WRITE(ksum)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_KSUM, ksum);
}

PARAM_FUNC_CLIENT_READ(ksum)
{
    return param_client_read (self, service, DSP_OPCODE_SET_GET_KSUM, ksum);
}

/* Delta/Sigma TBT threshold calculation functions */
PARAM_FUNC_CLIENT_WRITE(ds_tbt_thres)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_DS_TBT_THRES, ds_tbt_thres);
}

PARAM_FUNC_CLIENT_READ(ds_tbt_thres)
{
    return param_client_read (self, service, DSP_OPCODE_SET_GET_DS_TBT_THRES, ds_tbt_thres);
}

/* Delta/Sigma FOFB threshold calculation functions */
PARAM_FUNC_CLIENT_WRITE(ds_fofb_thres)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_DS_FOFB_THRES, ds_fofb_thres);
}

PARAM_FUNC_CLIENT_READ(ds_fofb_thres)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_DS_FOFB_THRES, ds_fofb_thres);
}

/* Delta/Sigma MONIT. threshold calculation functions */
PARAM_FUNC_CLIENT_WRITE(ds_monit_thres)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_DS_MONIT_THRES, ds_monit_thres);
}

PARAM_FUNC_CLIENT_READ(ds_monit_thres)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_DS_MONIT_THRES, ds_monit_thres);
}

/* Monitoring Amplitude channel 0 value */
PARAM_FUNC_CLIENT_WRITE(monit_amp_ch0)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH0, monit_amp_ch0);
}

PARAM_FUNC_CLIENT_READ(monit_amp_ch0)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH0, monit_amp_ch0);
}

/* Monitoring Amplitude channel 1 value */
PARAM_FUNC_CLIENT_WRITE(monit_amp_ch1)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH1, monit_amp_ch1);
}

PARAM_FUNC_CLIENT_READ(monit_amp_ch1)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH1, monit_amp_ch1);
}

/* Monitoring Amplitude channel 2 value */
PARAM_FUNC_CLIENT_WRITE(monit_amp_ch2)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH2, monit_amp_ch2);
}

PARAM_FUNC_CLIENT_READ(monit_amp_ch2)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH2, monit_amp_ch2);
}

/* Monitoring Amplitude channel 3 value */
PARAM_FUNC_CLIENT_WRITE(monit_amp_ch3)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH3, monit_amp_ch3);
}

PARAM_FUNC_CLIENT_READ(monit_amp_ch3)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_AMP_CH3, monit_amp_ch3);
}

/* Monitoring Position X value */
PARAM_FUNC_CLIENT_WRITE(monit_pos_x)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_POS_X, monit_pos_x);
}

PARAM_FUNC_CLIENT_READ(monit_pos_x)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_POS_X, monit_pos_x);
}

/* Monitoring Position Y value */
PARAM_FUNC_CLIENT_WRITE(monit_pos_y)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_POS_Y, monit_pos_y);
}

PARAM_FUNC_CLIENT_READ(monit_pos_y)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_POS_Y, monit_pos_y);
}

/* Monitoring Position Q value */
PARAM_FUNC_CLIENT_WRITE(monit_pos_q)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_POS_Q, monit_pos_q);
}

PARAM_FUNC_CLIENT_READ(monit_pos_q)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_POS_Q, monit_pos_q);
}

/* Monitoring Position Sum value */
PARAM_FUNC_CLIENT_WRITE(monit_pos_sum)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_POS_SUM, monit_pos_sum);
}

PARAM_FUNC_CLIENT_READ(monit_pos_sum)
{
     return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_POS_SUM, monit_pos_sum);
}

/* Monitoring Update value */
PARAM_FUNC_CLIENT_WRITE(monit_updt)
{
    return param_client_write (self, service, DSP_OPCODE_SET_GET_MONIT_UPDT, monit_updt);
}

PARAM_FUNC_CLIENT_READ(monit_updt)
{
    return param_client_read (self, service, DSP_OPCODE_SET_GET_MONIT_UPDT, monit_updt);
}

/**************** Swap SMIO Functions ****************/

/* Switching functions */
PARAM_FUNC_CLIENT_WRITE(sw)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_SW, sw);
}

PARAM_FUNC_CLIENT_READ(sw)
{
    return param_client_read (self, service, SWAP_OPCODE_SET_GET_SW, sw);
}

/* Switching enabling functions */
PARAM_FUNC_CLIENT_WRITE(sw_en)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_SW_EN, sw_en);
}

PARAM_FUNC_CLIENT_READ(sw_en)
{
    return param_client_read (self, service, SWAP_OPCODE_SET_GET_SW_EN, sw_en);
}

/* Switching Clock division functions */
PARAM_FUNC_CLIENT_WRITE(div_clk)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_DIV_CLK, div_clk);
}

PARAM_FUNC_CLIENT_READ(div_clk)
{
    return param_client_read (self, service, SWAP_OPCODE_SET_GET_DIV_CLK, div_clk);
}

/* Switching delay functions */
PARAM_FUNC_CLIENT_WRITE(sw_dly)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_SW_DLY, sw_dly);
}

PARAM_FUNC_CLIENT_READ(sw_dly)
{
    return param_client_read (self, service, SWAP_OPCODE_SET_GET_SW_DLY, sw_dly);
}

/* Windowing functions */
/* FIXME: Change client function wdw () to wdw_en (), which is a more semantic name */
PARAM_FUNC_CLIENT_WRITE(wdw)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_WDW_EN, wdw);
}

/* FIXME: Change client function wdw () to wdw_en (), which is a more semantic name */
PARAM_FUNC_CLIENT_READ(wdw)
{
    return param_client_read (self, service, SWAP_OPCODE_SET_GET_WDW_EN, wdw);
}

PARAM_FUNC_CLIENT_WRITE(wdw_dly)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_WDW_DLY, wdw_dly);
}

PARAM_FUNC_CLIENT_READ(wdw_dly)
{
    return param_client_read (self, service, SWAP_OPCODE_SET_GET_WDW_DLY, wdw_dly);
}

/* Gain functions */
/* TODO: reduce code repetition by, possibilly, group the OPCODES in
 * structure and merge all functions in a single
 * generic one for all channels (A, B, C, D) */
PARAM_FUNC_CLIENT_WRITE2(gain_a, dir_gain, inv_gain)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_GAIN_A,
            RW_SWAP_GAIN_UPPER_W(inv_gain) | RW_SWAP_GAIN_LOWER_W(dir_gain));
}

PARAM_FUNC_CLIENT_READ2(gain_a, dir_gain, inv_gain)
{
    uint32_t gain;
    halcs_client_err_e err = param_client_read (self, service,
            SWAP_OPCODE_SET_GET_GAIN_A, &gain);

    *dir_gain = RW_SWAP_GAIN_LOWER_R(gain);
    *inv_gain = RW_SWAP_GAIN_UPPER_R(gain);

    return err;
}

PARAM_FUNC_CLIENT_WRITE2(gain_b, dir_gain, inv_gain)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_GAIN_B,
            RW_SWAP_GAIN_UPPER_W(inv_gain) | RW_SWAP_GAIN_LOWER_W(dir_gain));
}

PARAM_FUNC_CLIENT_READ2(gain_b, dir_gain, inv_gain)
{
    uint32_t gain;
    halcs_client_err_e err = param_client_read (self, service,
            SWAP_OPCODE_SET_GET_GAIN_B, &gain);

    *dir_gain = RW_SWAP_GAIN_LOWER_R(gain);
    *inv_gain = RW_SWAP_GAIN_UPPER_R(gain);

    return err;
}

PARAM_FUNC_CLIENT_WRITE2(gain_c, dir_gain, inv_gain)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_GAIN_C,
            RW_SWAP_GAIN_UPPER_W(inv_gain) | RW_SWAP_GAIN_LOWER_W(dir_gain));
}

PARAM_FUNC_CLIENT_READ2(gain_c, dir_gain, inv_gain)
{
    uint32_t gain;
    halcs_client_err_e err = param_client_read (self, service,
            SWAP_OPCODE_SET_GET_GAIN_C, &gain);

    *dir_gain = RW_SWAP_GAIN_LOWER_R(gain);
    *inv_gain = RW_SWAP_GAIN_UPPER_R(gain);

    return err;
}

PARAM_FUNC_CLIENT_WRITE2(gain_d, dir_gain, inv_gain)
{
    return param_client_write (self, service, SWAP_OPCODE_SET_GET_GAIN_D,
            RW_SWAP_GAIN_UPPER_W(inv_gain) | RW_SWAP_GAIN_LOWER_W(dir_gain));
}

PARAM_FUNC_CLIENT_READ2(gain_d, dir_gain, inv_gain)
{
    uint32_t gain;
    halcs_client_err_e err = param_client_read (self, service,
            SWAP_OPCODE_SET_GET_GAIN_D, &gain);

    *dir_gain = RW_SWAP_GAIN_LOWER_R(gain);
    *inv_gain = RW_SWAP_GAIN_UPPER_R(gain);

    return err;
}

/**************** RFFE SMIO Functions ****************/

/* RFFE get/set attenuator */
PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_att)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_ATT,
            rffe_att);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_att)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_ATT,
            rffe_att);
}

/* RFFE get temperatures */
PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_temp_ac)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_TEMP_AC,
            rffe_temp_ac);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_temp_bd)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_TEMP_BD,
            rffe_temp_bd);
}

/* RFFE get/set set points */
PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_set_point_ac)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_SET_POINT_AC,
            rffe_set_point_ac);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_set_point_ac)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_SET_POINT_AC,
            rffe_set_point_ac);
}

PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_set_point_bd)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_SET_POINT_BD,
            rffe_set_point_bd);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_set_point_bd)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_SET_POINT_BD,
            rffe_set_point_bd);
}

/* RFFE get/set temperature control */
PARAM_FUNC_CLIENT_WRITE_BYTE(rffe_temp_control)
{
    return param_client_write_byte (self, service, RFFE_OPCODE_SET_GET_TEMP_CONTROL,
            rffe_temp_control);
}

PARAM_FUNC_CLIENT_READ_BYTE(rffe_temp_control)
{
    return param_client_read_byte (self, service, RFFE_OPCODE_SET_GET_TEMP_CONTROL,
            rffe_temp_control);
}

/* RFFE outputs */
PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_heater_ac)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_HEATER_AC,
            rffe_heater_ac);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_heater_ac)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_HEATER_AC,
            rffe_heater_ac);
}

PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_heater_bd)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_HEATER_BD,
            rffe_heater_bd);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_heater_bd)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_HEATER_BD,
            rffe_heater_bd);
}

/* RFFE get/set reset */
PARAM_FUNC_CLIENT_WRITE_BYTE(rffe_reset)
{
    return param_client_write_byte (self, service, RFFE_OPCODE_SET_GET_RESET,
            rffe_reset);
}

PARAM_FUNC_CLIENT_READ_BYTE(rffe_reset)
{
    return param_client_read_byte (self, service, RFFE_OPCODE_SET_GET_RESET,
            rffe_reset);
}

/* RFFE get/set reprogram */
PARAM_FUNC_CLIENT_WRITE_BYTE(rffe_reprog)
{
    return param_client_write_byte (self, service, RFFE_OPCODE_SET_GET_REPROG,
            rffe_reprog);
}

PARAM_FUNC_CLIENT_READ_BYTE(rffe_reprog)
{
    return param_client_read_byte (self, service, RFFE_OPCODE_SET_GET_REPROG,
            rffe_reprog);
}

/* RFFE set/get data */
halcs_client_err_e halcs_set_rffe_data (halcs_client_t *self, char *service,
        struct _smio_rffe_data_block_t *rffe_data_block)
{
    uint32_t rw = WRITE_MODE;
    return param_client_write_gen (self, service, RFFE_OPCODE_SET_GET_DATA,
            rw, rffe_data_block, sizeof (*rffe_data_block), NULL, 0);
}

halcs_client_err_e halcs_get_rffe_data (halcs_client_t *self, char *service,
        struct _smio_rffe_data_block_t *rffe_data_block)
{
    uint32_t rw = READ_MODE;
    return param_client_read_gen (self, service, RFFE_OPCODE_SET_GET_DATA,
            rw, rffe_data_block, sizeof (*rffe_data_block), NULL, 0,
            rffe_data_block, sizeof (*rffe_data_block));
}

/* RFFE get version */
halcs_client_err_e halcs_get_rffe_version (halcs_client_t *self, char *service,
        struct _smio_rffe_version_t *rffe_version)
{
    uint32_t rw = READ_MODE;
    return param_client_read_gen (self, service, RFFE_OPCODE_SET_GET_VERSION,
            rw, rffe_version, sizeof (*rffe_version), NULL, 0,
            rffe_version, sizeof (*rffe_version));
}

/* RFFE PID parameters */
PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_pid_ac_kp)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_PID_AC_KP,
            rffe_pid_ac_kp);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_pid_ac_kp)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_PID_AC_KP,
            rffe_pid_ac_kp);
}

PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_pid_ac_ti)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_PID_AC_TI,
            rffe_pid_ac_ti);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_pid_ac_ti)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_PID_AC_TI,
            rffe_pid_ac_ti);
}

PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_pid_ac_td)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_PID_AC_TD,
            rffe_pid_ac_td);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_pid_ac_td)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_PID_AC_TD,
            rffe_pid_ac_td);
}

PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_pid_bd_kp)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_PID_BD_KP,
            rffe_pid_bd_kp);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_pid_bd_kp)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_PID_BD_KP,
            rffe_pid_bd_kp);
}

PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_pid_bd_ti)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_PID_BD_TI,
            rffe_pid_bd_ti);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_pid_bd_ti)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_PID_BD_TI,
            rffe_pid_bd_ti);
}

PARAM_FUNC_CLIENT_WRITE_DOUBLE(rffe_pid_bd_td)
{
    return param_client_write_double (self, service, RFFE_OPCODE_SET_GET_PID_BD_TD,
            rffe_pid_bd_td);
}

PARAM_FUNC_CLIENT_READ_DOUBLE(rffe_pid_bd_td)
{
    return param_client_read_double (self, service, RFFE_OPCODE_SET_GET_PID_BD_TD,
            rffe_pid_bd_td);
}

/********************** AFC Diagnostics Functions ********************/

/* AFC card slot */
PARAM_FUNC_CLIENT_WRITE(afc_diag_card_slot)
{
    return param_client_write (self, service, AFC_DIAG_OPCODE_SET_GET_CARD_SLOT,
            afc_diag_card_slot);
}

PARAM_FUNC_CLIENT_READ(afc_diag_card_slot)
{
    return param_client_read (self, service, AFC_DIAG_OPCODE_SET_GET_CARD_SLOT,
            afc_diag_card_slot);
}

/* AFC IPMI address */
PARAM_FUNC_CLIENT_WRITE(afc_diag_ipmi_addr)
{
    return param_client_write (self, service, AFC_DIAG_OPCODE_SET_GET_IPMI_ADDR,
            afc_diag_ipmi_addr);
}

PARAM_FUNC_CLIENT_READ(afc_diag_ipmi_addr)
{
    return param_client_read (self, service, AFC_DIAG_OPCODE_SET_GET_IPMI_ADDR,
            afc_diag_ipmi_addr);
}

/* Build Revision */
halcs_client_err_e halcs_get_afc_diag_build_revision (halcs_client_t *self, char *service,
        struct _smio_afc_diag_revision_data_t *revision_data)
{
    uint32_t rw = READ_MODE;
    return param_client_read_gen (self, service, AFC_DIAG_OPCODE_GET_BUILD_REVISION,
            rw, revision_data, sizeof (*revision_data), NULL, 0,
            revision_data, sizeof (*revision_data));
}

/* Build Date */
halcs_client_err_e halcs_get_afc_diag_build_date (halcs_client_t *self, char *service,
        struct _smio_afc_diag_revision_data_t *revision_data)
{
    uint32_t rw = READ_MODE;
    return param_client_read_gen (self, service, AFC_DIAG_OPCODE_GET_BUILD_DATE,
            rw, revision_data, sizeof (*revision_data), NULL, 0,
            revision_data, sizeof (*revision_data));
}

/* Build User Name */
halcs_client_err_e halcs_get_afc_diag_build_user_name (halcs_client_t *self, char *service,
        struct _smio_afc_diag_revision_data_t *revision_data)
{
    uint32_t rw = READ_MODE;
    return param_client_read_gen (self, service, AFC_DIAG_OPCODE_GET_BUILD_USER_NAME,
            rw, revision_data, sizeof (*revision_data), NULL, 0,
            revision_data, sizeof (*revision_data));
}

/* Build User Email */
halcs_client_err_e halcs_get_afc_diag_build_user_email (halcs_client_t *self, char *service,
        struct _smio_afc_diag_revision_data_t *revision_data)
{
    uint32_t rw = READ_MODE;
    return param_client_read_gen (self, service, AFC_DIAG_OPCODE_GET_BUILD_USER_EMAIL,
            rw, revision_data, sizeof (*revision_data), NULL, 0,
            revision_data, sizeof (*revision_data));
}

/********************** Trigger Interface Functions ********************/

/* Trigger direction */
PARAM_FUNC_CLIENT_WRITE2(trigger_dir, chan, dir)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_DIR,
            chan, dir);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_dir, chan, dir)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_DIR,
            chan, dir);
}

/* Trigger direction */
PARAM_FUNC_CLIENT_WRITE2(trigger_dir_pol, chan, dir_pol)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_DIR_POL,
            chan, dir_pol);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_dir_pol, chan, dir_pol)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_DIR_POL,
                chan, dir_pol);
}

/* Trigger receive counter reset */
PARAM_FUNC_CLIENT_WRITE2(trigger_rcv_count_rst, chan, rcv_count_rst)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_RCV_COUNT_RST,
            chan, rcv_count_rst);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_rcv_count_rst, chan, rcv_count_rst)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_RCV_COUNT_RST,
            chan, rcv_count_rst);
}

/* Trigger transmit counter reset */
PARAM_FUNC_CLIENT_WRITE2(trigger_transm_count_rst, chan, transm_count_rst)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_TRANSM_COUNT_RST,
            chan, transm_count_rst);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_transm_count_rst, chan, transm_count_rst)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_TRANSM_COUNT_RST,
            chan, transm_count_rst);
}

/* Trigger receive length debounce */
PARAM_FUNC_CLIENT_WRITE2(trigger_rcv_len, chan, rcv_len)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_RCV_LEN,
            chan, rcv_len);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_rcv_len, chan, rcv_len)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_RCV_LEN,
            chan, rcv_len);
}

/* Trigger transmit length debounce */
PARAM_FUNC_CLIENT_WRITE2(trigger_transm_len, chan, transm_len)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_TRANSM_LEN,
            chan, transm_len);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_transm_len, chan, transm_len)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_TRANSM_LEN,
            chan, transm_len);
}

/* Trigger count_receive */
PARAM_FUNC_CLIENT_WRITE2(trigger_count_rcv, chan, count_rcv)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_COUNT_RCV,
            chan, count_rcv);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_count_rcv, chan, count_rcv)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_COUNT_RCV,
            chan, count_rcv);
}

/* Trigger count transmit */
PARAM_FUNC_CLIENT_WRITE2(trigger_count_transm, chan, count_transm)
{
    return param_client_write2 (self, service, TRIGGER_IFACE_OPCODE_COUNT_TRANSM,
            chan, count_transm);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_count_transm, chan, count_transm)
{
    return param_client_write_read (self, service, TRIGGER_IFACE_OPCODE_COUNT_TRANSM,
            chan, count_transm);
}

/********************** Trigger Mux Functions ********************/

/* Trigger receive source */
PARAM_FUNC_CLIENT_WRITE2(trigger_rcv_src, chan, rcv_src)
{
    return param_client_write2 (self, service, TRIGGER_MUX_OPCODE_RCV_SRC,
            chan, rcv_src);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_rcv_src, chan, rcv_src)
{
    return param_client_write_read (self, service, TRIGGER_MUX_OPCODE_RCV_SRC,
            chan, rcv_src);
}

/* Trigger receive in selection */
PARAM_FUNC_CLIENT_WRITE2(trigger_rcv_in_sel, chan, rcv_in_sel)
{
    return param_client_write2 (self, service, TRIGGER_MUX_OPCODE_RCV_IN_SEL,
            chan, rcv_in_sel);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_rcv_in_sel, chan, rcv_in_sel)
{
    return param_client_write_read (self, service, TRIGGER_MUX_OPCODE_RCV_IN_SEL,
            chan, rcv_in_sel);
}

/* Trigger transmit source */
PARAM_FUNC_CLIENT_WRITE2(trigger_transm_src, chan, transm_src)
{
    return param_client_write2 (self, service, TRIGGER_MUX_OPCODE_TRANSM_SRC,
            chan, transm_src);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_transm_src, chan, transm_src)
{
    return param_client_write_read (self, service, TRIGGER_MUX_OPCODE_TRANSM_SRC,
            chan, transm_src);
}

/* Trigger transmit selection */
PARAM_FUNC_CLIENT_WRITE2(trigger_transm_out_sel, chan, transm_out_sel)
{
    return param_client_write2 (self, service, TRIGGER_MUX_OPCODE_TRANSM_OUT_SEL,
            chan, transm_out_sel);
}

PARAM_FUNC_CLIENT_WRITE_READ(trigger_transm_out_sel, chan, transm_out_sel)
{
    return param_client_write_read (self, service, TRIGGER_MUX_OPCODE_TRANSM_OUT_SEL,
            chan, transm_out_sel);
}

/**************** Helper Function ****************/

halcs_client_err_e func_polling (halcs_client_t *self, char *name, char *service,
        uint32_t *input, uint32_t *output, int timeout)
{
    return _func_polling (self, name, service, input, output, timeout);
}

/* Polling Function */
static halcs_client_err_e _func_polling (halcs_client_t *self, char *name,
        char *service, uint32_t *input, uint32_t *output, int timeout)
{
    assert (self);
    assert (name);
    assert (service);

    /* timeout < 0 means "infinite" wait */
    if (timeout < 0) {
        /* FIXME! Very unportable way! We assume that time_t is at least of
         * size INT */
        timeout = INT_MAX;
    }

    halcs_client_err_e err = HALCS_CLIENT_SUCCESS;
    time_t start = time (NULL);
    while ((time(NULL) - start)*1000 < timeout) {
        if (zsys_interrupted) {
            err = HALCS_CLIENT_INT;
            goto halcs_zsys_interrupted;
        }

        const disp_op_t* func = halcs_func_translate(name);
        err = halcs_func_exec (self, func, service, input, output);

        if (err == HALCS_CLIENT_SUCCESS) {
            DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] "
                    "func_polling: finished waiting\n");
            goto exit;
        }

        usleep (MSECS*MIN_WAIT_TIME);
    }

    DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] "
            "func_polling: number of tries was exceeded\n");
    /* timeout occured */
    err = HALCS_CLIENT_ERR_TIMEOUT;

halcs_zsys_interrupted:
exit:
    return err;
}
