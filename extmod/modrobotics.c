// SPDX-License-Identifier: MIT
// Copyright (c) 2019 Laurens Valk

#include <math.h>

#include <pbio/motor.h>

#include "py/mphal.h"
#include "py/runtime.h"
#include "pberror.h"
#include "pbobj.h"
#include "modmotor.h"
#include "pbthread.h"


// Class structure for DriveBase
typedef struct _robotics_DriveBase_obj_t {
    mp_obj_base_t base;
    pbio_port_t port_left;
    pbio_port_t port_right;
    float_t wheel_diameter;
    float_t axle_track;
} robotics_DriveBase_obj_t;

STATIC mp_obj_t robotics_DriveBase_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args ) {
    robotics_DriveBase_obj_t *self = m_new_obj(robotics_DriveBase_obj_t);
    self->base.type = (mp_obj_type_t*) type;

    // We should have four arguments
    mp_arg_check_num(n_args, n_kw, 4, 4, false);

    // Argument must be two motors and two dimensions
    if (!MP_OBJ_IS_TYPE(args[0], &motor_Motor_type) || !MP_OBJ_IS_TYPE(args[1], &motor_Motor_type)) {
        pb_assert(PBIO_ERROR_INVALID_ARG);
    }
    self->port_left = get_port(args[0]);
    self->port_right = get_port(args[1]);

    // Argument must be two unique motors
    if (self->port_left == self->port_right) {
        pb_assert(PBIO_ERROR_INVALID_ARG);
    }

    // Motors should still be connected and should have encoders, which we can test by reading their angles
    if (self->port_left == self->port_right) {
        int32_t dummy_angle;
        pb_assert(pbio_motor_get_angle(self->port_left, &dummy_angle));
        pb_assert(pbio_motor_get_angle(self->port_right, &dummy_angle));
    }

    // Get wheel diameter and axle track dimensions
    self->wheel_diameter = mp_obj_get_num(args[2]);
    self->axle_track = mp_obj_get_num(args[3]);

    // Assert that the dimensions are positive
    if (self->wheel_diameter < 1 || self->axle_track < 1) {
        pb_assert(PBIO_ERROR_INVALID_ARG);
    }

    return MP_OBJ_FROM_PTR(self);
}

STATIC void robotics_DriveBase_print(const mp_print_t *print,  mp_obj_t self_in, mp_print_kind_t kind) {
    robotics_DriveBase_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, qstr_str(MP_QSTR_DriveBase));
    mp_printf(print, " with left motor on Port %c and right motor on Port %c", self->port_left, self->port_right);
}

STATIC mp_obj_t robotics_DriveBase_drive(mp_obj_t self_in, mp_obj_t speed, mp_obj_t steering) {
    robotics_DriveBase_obj_t *self = MP_OBJ_TO_PTR(self_in);
    float sum = mp_obj_get_num(speed)/self->wheel_diameter*(720.0/M_PI);
    float dif = 2*self->axle_track/self->wheel_diameter*mp_obj_get_num(steering);

    pb_thread_enter();

    pbio_error_t err_left = pbio_motor_run(self->port_left, (sum+dif)/2);
    pbio_error_t err_right = pbio_motor_run(self->port_right, (sum-dif)/2);

    pb_thread_exit();

    pb_assert(err_left);
    pb_assert(err_right);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(robotics_DriveBase_drive_obj, robotics_DriveBase_drive);

STATIC mp_obj_t robotics_DriveBase_stop(size_t n_args, const mp_obj_t *args){
    // Parse arguments and/or set default optional arguments
    robotics_DriveBase_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    pbio_motor_after_stop_t after_stop = n_args > 1 ? mp_obj_get_int(args[1]) : PBIO_MOTOR_STOP_COAST;
    pbio_error_t err_left, err_right;

    pb_thread_enter();

    err_left = pbio_motor_stop(self->port_left, after_stop);
    err_right = pbio_motor_stop(self->port_right, after_stop);

    pb_thread_exit();

    pb_assert(err_left);
    pb_assert(err_right);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(robotics_DriveBase_stop_obj, 1, 2, robotics_DriveBase_stop);

STATIC mp_obj_t robotics_DriveBase_drive_time(size_t n_args, const mp_obj_t *args){
    robotics_DriveBase_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    robotics_DriveBase_drive(self, args[1], args[2]);
    mp_hal_delay_ms(mp_obj_get_num(args[3]));
    // TODO: parse stop type
    robotics_DriveBase_drive(self, MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_NEW_SMALL_INT(0));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(robotics_DriveBase_drive_time_obj, 4, 5, robotics_DriveBase_drive_time);

/*
DriveBase class tables
*/
STATIC const mp_rom_map_elem_t robotics_DriveBase_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_drive), MP_ROM_PTR(&robotics_DriveBase_drive_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&robotics_DriveBase_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_drive_time), MP_ROM_PTR(&robotics_DriveBase_drive_time_obj) },
};
STATIC MP_DEFINE_CONST_DICT(robotics_DriveBase_locals_dict, robotics_DriveBase_locals_dict_table);

STATIC const mp_obj_type_t robotics_DriveBase_type = {
    { &mp_type_type },
    .name = MP_QSTR_DriveBase,
    .print = robotics_DriveBase_print,
    .make_new = robotics_DriveBase_make_new,
    .locals_dict = (mp_obj_dict_t*)&robotics_DriveBase_locals_dict,
};

/*
robotics module tables
*/

STATIC const mp_rom_map_elem_t robotics_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_robotics)         },
    { MP_ROM_QSTR(MP_QSTR_DriveBase),   MP_ROM_PTR(&robotics_DriveBase_type)  },
};
STATIC MP_DEFINE_CONST_DICT(pb_module_robotics_globals, robotics_globals_table);

const mp_obj_module_t pb_module_robotics = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&pb_module_robotics_globals,
};

