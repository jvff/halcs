/*
 * Copyright (C) 2014 LNLS (www.lnls.br)
 * Author: Lucas Russo <lucas.russo@lnls.br>
 *
 * Released according to the GNU LGPL, version 3 or any later version.
 */

#include "hal_assert.h"

#include "bpm_client.h"
#include "rw_param_client.h"
#include "rw_param_codes.h"

/* Undef ASSERT_ALLOC to avoid conflicting with other ASSERT_ALLOC */
#ifdef ASSERT_TEST
#undef ASSERT_TEST
#endif
#define ASSERT_TEST(test_boolean, err_str, err_goto_label, /* err_core */ ...) \
    ASSERT_HAL_TEST(test_boolean, LIB_CLIENT, "[libclient:rw_param_client]", \
            err_str, err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef ASSERT_ALLOC
#undef ASSERT_ALLOC
#endif
#define ASSERT_ALLOC(ptr, err_goto_label, /* err_core */ ...)   	\
    ASSERT_HAL_ALLOC(ptr, LIB_CLIENT, "[libclient:rw_param_client]", \
            bpm_client_err_str(BPM_CLIENT_ERR_ALLOC),               \
            err_goto_label, /* err_core */ __VA_ARGS__)

#ifdef CHECK_ERR
#undef CHECK_ERR
#endif
#define CHECK_ERR(err, err_type)                                    \
    CHECK_HAL_ERR(err, LIB_CLIENT, "[libclient:rw_param_client]",   \
            bpm_client_err_str (err_type))

bpm_client_err_e param_client_send_rw (bpm_client_t *self, char *service,
        uint32_t operation, uint32_t rw, uint32_t param)
{
    bpm_client_err_e err = BPM_CLIENT_SUCCESS;

    zmsg_t *request = zmsg_new ();
    err = (request == NULL) ? BPM_CLIENT_ERR_ALLOC : err;
    ASSERT_ALLOC(request, err_send_msg_alloc);
    zmsg_addmem (request, &operation, sizeof (operation));
    zmsg_addmem (request, &rw, sizeof (rw));

    if (rw == WRITE_MODE) {
        zmsg_addmem (request, &param, sizeof (param));
    }

    mdp_client_send (self->mdp_client, service, &request);

err_send_msg_alloc:
    return err;
}

bpm_client_err_e param_client_recv_rw (bpm_client_t *self, char *service,
        zmsg_t **report)
{
    (void) service;
    assert (report);

    bpm_client_err_e err = BPM_CLIENT_SUCCESS;

    /* Receive report */
    *report = mdp_client_recv (self->mdp_client, NULL, NULL);
    err = (*report == NULL) ? BPM_CLIENT_ERR_SERVER : err;
    ASSERT_TEST(*report != NULL, "Could not receive message", err_null_msg);

err_null_msg:
    return err;
}

/* TODO: improve error handling */
bpm_client_err_e param_client_write (bpm_client_t *self, char *service,
        uint32_t operation, uint32_t param)
{
    assert (self);
    assert (service);

    bpm_client_err_e err = BPM_CLIENT_SUCCESS;
    uint32_t rw = WRITE_MODE;
    zmsg_t *report;

    err = param_client_send_rw (self, service, operation, rw, param);
    ASSERT_TEST(err == BPM_CLIENT_SUCCESS, "Could not send message", err_send_msg);
    err = param_client_recv_rw (self, service, &report);
    ASSERT_TEST(err == BPM_CLIENT_SUCCESS, "Could not receive message", err_recv_msg);

    /* TODO: better handling of malformed messages */
    assert (zmsg_size (report) == 1);

    /* Message is:
     * frame 0: error code      */
    zframe_t *err_code = zmsg_pop (report);
    err = (err_code == NULL) ? BPM_CLIENT_ERR_SERVER : err;
    ASSERT_TEST(err_code != NULL, "Could not receive error code", err_null_code);

    if (*(RW_REPLY_TYPE *) zframe_data(err_code) != RW_WRITE_OK) {
        DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] rw_param_client: "
                "parameter set error, try again\n");
        err = BPM_CLIENT_ERR_AGAIN;
        goto err_set_param;
    }

err_set_param:
    zframe_destroy (&err_code);
err_null_code:
    zmsg_destroy (&report);
err_recv_msg:
err_send_msg:
    return err;
}

/* TODO: improve error handling */
bpm_client_err_e param_client_read (bpm_client_t *self, char *service,
        uint32_t operation, uint32_t *param_out)
{
    assert (self);
    assert (service);

    bpm_client_err_e err = BPM_CLIENT_SUCCESS;
    uint32_t rw = READ_MODE;
    zmsg_t *report;

    err = param_client_send_rw (self, service, operation, rw,
            0 /* in read mode this doesn't matter */);
    ASSERT_TEST(err == BPM_CLIENT_SUCCESS, "Could not send message", err_send_msg);
    err = param_client_recv_rw (self, service, &report);
    ASSERT_TEST(err == BPM_CLIENT_SUCCESS, "Could not receive message", err_recv_msg);

    /* TODO: better handling of malformed messages */
    assert (zmsg_size (report) == 2);

    /* Message is:
     * frame 0: error code
     * frame 1: data read */
    zframe_t *err_code = zmsg_pop(report);
    ASSERT_TEST(err_code != NULL, "Could not receive error code", err_null_code);
    zframe_t *data_out_frm = zmsg_pop(report);
    ASSERT_TEST(data_out_frm != NULL, "Could not receive parameter", err_null_param);

    if (*(RW_REPLY_TYPE *) zframe_data(err_code) != RW_READ_OK) {
        DBE_DEBUG (DBG_LIB_CLIENT | DBG_LVL_TRACE, "[libclient] rw_param_client:: "
                "paramter get error, try again\n");
        err = BPM_CLIENT_ERR_AGAIN;
        goto err_get_param;
    }

    *param_out = *(uint32_t *) zframe_data(data_out_frm);

err_get_param:
    zframe_destroy (&data_out_frm);
err_null_param:
    zframe_destroy (&err_code);
err_null_code:
    zmsg_destroy (&report);
err_recv_msg:
err_send_msg:
    return err;
}

