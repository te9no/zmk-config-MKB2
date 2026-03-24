/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_trixer

#include <stdlib.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/keymap.h>

/*
 * 6DoF Joystick Input Processor (Trixer)
 *
 * Fuses three MLX90393 magnetometers into a 6-axis joystick (x, y, z, rx, ry, rz).
 *
 * Hardware Configuration:
 * - Three magnetometer chips (MLX90393) on circular PCB at configured radius
 * - Chip Alpha (idx 0): 0° position (6 o'clock), die pin 1 at bottom right
 * - Chip Beta (idx 1): -120° position (2 o'clock), module edge faces outward
 * - Chip Gamma (idx 2): +120° position (10 o'clock), module edge faces outward
 * - Magnets glued on ring PCB, hovering 8mm above sensors
 *
 * Coordinate System: Z-up (right-handed, screen coords)
 *   +X: Right
 *   +Y: Down (screen coordinates, +Y is down for mouse)
 *   +Z: Up
 *
 * Processing Pipeline:
 * 1. Cache sensor readings from all three chips until sync
 * 2. Transform chip-local XY to world coordinates using rotation angles
 * 3. Calculate centroid (x, y, z translation) from averaged sensor positions
 * 4. Calculate pitch/roll from Z-differentials relative to centroid
 * 5. Calculate yaw from average cross product of radial and displacement vectors
 * 6. Apply weighted smoothing over recent samples (if configured)
 * 7. Apply per-axis deadzones and neutral state detection
 * 8. Report relative motion events at configured interval
 *
 * Rotation Calculations:
 *   - Pitch (around X): arcsin((z_alpha - (z_beta + z_gamma)/2) / radius)
 *   - Roll (around Y): arcsin((z_beta - z_gamma) / radius)
 *   - Yaw (around Z): avg(cross(radial, displacement)) / radius
 *
 * Chip mounting angles (for local-to-world transform):
 *   - Alpha: 0°
 *   - Beta: -120° (-2π/3)
 *   - Gamma: +120° (+2π/3)
 */

#define IDX_SEN_ALPHA   0
#define IDX_SEN_BETA    1
#define IDX_SEN_GAMMA   2
#define NUM_SENSORS     3
#define MIN_SENSOR_NEUTRAL 2  /* Min sensors in neutral to trigger device neutral (1-3) */

/* Chip sensor position */
#define ALPHA_AT_SOUTH 1      /* Chip Alpha on South (6 o'clock) */
#define ALPHA_AT_NORTH 0      /* Chip Alpha on North (12 o'clock) */
BUILD_ASSERT((ALPHA_AT_SOUTH || ALPHA_AT_NORTH) && ALPHA_AT_SOUTH != ALPHA_AT_NORTH,
            "Must choose either ALPHA_AT_SOUTH or ALPHA_AT_NORTH");

#ifndef M_PI
#define M_PI (3.14159265358979323846f)
#endif

#define RAD_TO_DEG (180.0f / M_PI)
#define SQRT3 (1.732050808f)

static const float chip_angles_rad[NUM_SENSORS] = {
    [IDX_SEN_ALPHA] = 0.0f,
    [IDX_SEN_BETA]  = -2.0f * M_PI / 3.0f,
    [IDX_SEN_GAMMA] = 2.0f * M_PI / 3.0f
};

/* 3D vector for intermediate calculations */
struct vec3 {
    float x, y, z;
};

struct zip_trixer_config {
    uint32_t sync_report_ms;
    uint32_t radius_um;
    uint32_t xy_sensitivity_num;
    uint32_t xy_sensitivity_denom;
    uint32_t z_sensitivity_num;
    uint32_t z_sensitivity_denom;
    uint32_t pitch_scale_num;
    uint32_t pitch_scale_denom;
    uint32_t roll_scale_num;
    uint32_t roll_scale_denom;
    uint32_t yaw_scale_num;
    uint32_t yaw_scale_denom;
    uint32_t yaw_comp_x_num;
    uint32_t yaw_comp_x_denom;
    uint32_t neutral_timeout_ms;
    uint8_t smooth_len;
    uint16_t rpt_dzn_x, rpt_dzn_y, rpt_dzn_z;
    uint16_t rpt_dzn_rx, rpt_dzn_ry, rpt_dzn_rz;
};

struct zip_trixer_sensor_rel {
    int16_t x, y, z;
};

struct zip_trixer_data {
    const struct device *dev;
    struct zip_trixer_sensor_rel caches[NUM_SENSORS];
    bool caches_valid[NUM_SENSORS];
    int64_t last_rpt_time;

    /* Scheduled work for sending neutral report */
    struct k_work_delayable neutral_work;
    bool neutral_pending;
    bool in_neutral_state;

    /* Accumulated output values (scaled and ready to report) */
    int16_t x, y, z;        /* Translation: centroid of three magnet positions */
    int16_t rx, ry, rz;     /* Rotation: pitch (X), roll (Y), yaw (Z) in degrees */

    /* Smoothed output history for delta smoothing */
    int16_t *smooth_x, *smooth_y, *smooth_z;
    int16_t *smooth_rx, *smooth_ry, *smooth_rz;
    uint8_t smooth_filled;
    uint8_t smooth_idx;
};

/* Rotate 2D vector counter-clockwise by given angle (radians) */
static inline void rotate2d(float *x, float *y, float angle)
{
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    float x_new = (*x) * cos_a - (*y) * sin_a;
    float y_new = (*x) * sin_a + (*y) * cos_a;
    *x = x_new;
    *y = y_new;
}

/* Forward declaration */
static void neutral_work_handler(struct k_work *work);

static int trixer_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                           uint32_t param2, struct zmk_input_processor_state *state)
{
    const struct zip_trixer_config *config = dev->config;
    struct zip_trixer_data *data = dev->data;

    /* Validate sensor index */
    if (param1 >= NUM_SENSORS) {
        return ZMK_INPUT_PROC_STOP;
    }

    /* Store sensor reading in cache */
    if (event->code == INPUT_REL_X) {
        data->caches[param1].x = event->value;
    } else if (event->code == INPUT_REL_Y) {
        data->caches[param1].y = event->value;
    } else if (event->code == INPUT_REL_Z) {
        data->caches[param1].z = event->value;
    }
    
    if (event->sync) {
        data->caches_valid[param1] = true;
    }

    /*
     * Count sensors in neutral (reporting zero) and check for any motion.
     * Device enters neutral when at least MIN_SENSOR_NEUTRAL sensors are idle.
     * Any motion cancels pending neutral and exits neutral state immediately.
     * This check happens before requiring all sensors to allow partial neutral detection.
     */
    int neutral_count = 0;
    bool any_motion = false;
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (data->caches[i].x != 0 || data->caches[i].y != 0 || data->caches[i].z != 0) {
            any_motion = true;
        } else {
            neutral_count++;
        }
    }

    if (any_motion) {
        /* Cancel any pending neutral timeout */
        if (data->neutral_pending) {
            k_work_cancel_delayable(&data->neutral_work);
            data->neutral_pending = false;
        }

        /* Exit neutral state if we were in it */
        if (data->in_neutral_state) {
            // LOG_DBG("NEUTRAL exited (motion detected)");
            data->in_neutral_state = false;
        }
    } else if (neutral_count >= MIN_SENSOR_NEUTRAL) {
        /*
         * Sufficient sensors in neutral - schedule neutral report after timeout.
         * This handles the case where sensors enter deadzone at different times
         * by waiting for the full timeout period after the last sync.
         */
        if (!data->neutral_pending && !data->in_neutral_state) {
            k_work_schedule(&data->neutral_work, K_MSEC(config->neutral_timeout_ms));
            data->neutral_pending = true;
        }

        /* Clear caches and wait for next cycle or timeout */
        for (int i = 0; i < NUM_SENSORS; i++) {
            data->caches_valid[i] = false;
            data->caches[i].x = 0;
            data->caches[i].y = 0;
            data->caches[i].z = 0;
        }
        return ZMK_INPUT_PROC_STOP;
    }

    /* Process when all sensors have valid data for 6DoF calculations */
    if (!data->caches_valid[IDX_SEN_ALPHA] ||
        !data->caches_valid[IDX_SEN_BETA] ||
        !data->caches_valid[IDX_SEN_GAMMA]) {
        return ZMK_INPUT_PROC_STOP;
    }

    /* Transform chip-local readings to world coordinates */
    struct vec3 magnet_world[NUM_SENSORS];  /* XY in world frame, Z remains local */
    
    float radius_mm = config->radius_um / 1000.0f;
    float xy_scale = (float)config->xy_sensitivity_num / (float)config->xy_sensitivity_denom;
    float z_scale = (float)config->z_sensitivity_num / (float)config->z_sensitivity_denom;
    float pitch_scale = (float)config->pitch_scale_num / (float)config->pitch_scale_denom;
    float roll_scale = (float)config->roll_scale_num / (float)config->roll_scale_denom;
    float yaw_scale = (float)config->yaw_scale_num / (float)config->yaw_scale_denom;
    
    for (int i = 0; i < NUM_SENSORS; i++) {
        float local_x = (float)data->caches[i].x;
        float local_y = (float)data->caches[i].y;
        float local_z = (float)data->caches[i].z;
        
        float world_x = local_x;
        float world_y = local_y;
        rotate2d(&world_x, &world_y, chip_angles_rad[i]);
        
#if ALPHA_AT_NORTH
        world_x = -world_x;
        world_y = -world_y;
#endif

        magnet_world[i].x = world_x;
        magnet_world[i].y = world_y;
        magnet_world[i].z = local_z;
    }

    struct vec3 centroid;
    centroid.x = (magnet_world[0].x + magnet_world[1].x + magnet_world[2].x) / 3.0f;
    centroid.y = (magnet_world[0].y + magnet_world[1].y + magnet_world[2].y) / 3.0f;
    centroid.z = (magnet_world[0].z + magnet_world[1].z + magnet_world[2].z) / 3.0f;

    data->x = (int16_t)roundf(centroid.x * xy_scale);
    data->y = (int16_t)roundf(centroid.y * xy_scale);
    data->z = (int16_t)roundf(centroid.z);

    /* Pitch / Roll calculation using relative differential of altitude of magnets */
    float z_alpha_rel = (magnet_world[IDX_SEN_ALPHA].z - centroid.z) * z_scale;
    float z_beta_rel = (magnet_world[IDX_SEN_BETA].z - centroid.z) * z_scale;
    float z_gamma_rel = (magnet_world[IDX_SEN_GAMMA].z - centroid.z) * z_scale;

#if ALPHA_AT_SOUTH
    float sin_pitch = (z_alpha_rel - ((z_beta_rel + z_gamma_rel) / 2.0f)) / radius_mm;
    float sin_roll = (z_beta_rel - z_gamma_rel) / radius_mm;
#elif ALPHA_AT_NORTH
    float sin_pitch = (((z_beta_rel + z_gamma_rel) / 2.0f) - z_alpha_rel) / radius_mm;
    float sin_roll = (z_gamma_rel - z_beta_rel) / radius_mm;
#endif

    if (sin_pitch > 1.0f) sin_pitch = 1.0f;
    if (sin_pitch < -1.0f) sin_pitch = -1.0f;
    if (sin_roll > 1.0f) sin_roll = 1.0f;
    if (sin_roll < -1.0f) sin_roll = -1.0f;

    float pitch_rad = asinf(sin_pitch);
    float roll_rad = asinf(sin_roll);

    data->rx = (int16_t)roundf(pitch_rad * RAD_TO_DEG * pitch_scale);
    data->ry = (int16_t)roundf(roll_rad * RAD_TO_DEG * roll_scale);

    /*
     * Yaw calculation using weighted cross product method.
     *
     * Yaw rotation moves each magnet tangentially around the Z axis. The tangential
     * displacement is measured by cross(radial_vector, displacement_vector), which
     * gives radius * yaw_angle for pure rotation while canceling pure translation.
     *
     * TRANSLATION COMPENSATION: When the centroid translates (X/Y), the magnetic
     * interference pattern shifts asymmetrically, causing unintended yaw rotation.
     * Configurable compensation factor (yaw_comp_x) adds the
     * translation influence to the average cross product.
     *
     * Radial vectors (unit vectors from center to each chip position):
     *   - Alpha (0°):       (0, 1)
     *   - Beta (-120°):     (-√3/2, -1/2)
     *   - Gamma (+120°):    (√3/2, -1/2)
     *
     * For each sensor: cross_z = radial_x * dy - radial_y * dx
     * Then: yaw = (avg(cross_z) + trans_comp) / radius
     */

    /* XY displacements from calibrated origin */
    float dx_alpha = magnet_world[IDX_SEN_ALPHA].x;
    // float dy_alpha = magnet_world[IDX_SEN_ALPHA].y;
    float dx_beta = magnet_world[IDX_SEN_BETA].x;
    float dy_beta = magnet_world[IDX_SEN_BETA].y;
    float dx_gamma = magnet_world[IDX_SEN_GAMMA].x;
    float dy_gamma = magnet_world[IDX_SEN_GAMMA].y;

    /* Cross products: radial × displacement (Z-component only) */
#if ALPHA_AT_SOUTH
    float cross_alpha = -1.0f * dx_alpha;
    float cross_beta = -((-SQRT3 / 2.0f) * dy_beta - (-0.5f) * dx_beta);
    float cross_gamma = -((SQRT3 / 2.0f) * dy_gamma - (-0.5f) * dx_gamma);
#elif ALPHA_AT_NORTH
    float cross_alpha = 1.0f * dx_alpha;
    float cross_beta = ((-SQRT3 / 2.0f) * dy_beta - (-0.5f) * dx_beta);
    float cross_gamma = ((SQRT3 / 2.0f) * dy_gamma - (-0.5f) * dx_gamma);
#endif

    /* Average tangential displacement */
    float avg_cross = (cross_alpha + cross_beta + cross_gamma) / 3.0f;

    /* Translation compensation: add centroid X influence to yaw
     * When centroid translates, magnetic interference shifts and causes
     * unintended rotation. Compensation factor tunes this correction. */
    float yaw_comp_x = (float)config->yaw_comp_x_num / (float)config->yaw_comp_x_denom;
    float trans_comp = yaw_comp_x * centroid.x;
    // LOG_DBG("yaw comp: x=%.3f trans_comp=%.3f centroid.x=%.1f centroid.y=%.1f",
    //         yaw_comp_x, trans_comp, centroid.x, centroid.y);

#if ALPHA_AT_SOUTH
    float yaw_rad = (avg_cross + trans_comp) / radius_mm;
#elif ALPHA_AT_NORTH
    float yaw_rad = (avg_cross - trans_comp) / radius_mm;
#endif

    /* Clamp to [-π, π] range */
    if (yaw_rad > M_PI) yaw_rad = M_PI;
    if (yaw_rad < -M_PI) yaw_rad = -M_PI;

    data->rz = (int16_t)roundf(yaw_rad * RAD_TO_DEG * yaw_scale);

    /* Reset caches for next sensor cycle */
    for (int i = 0; i < NUM_SENSORS; i++) {
        data->caches_valid[i] = false;
        data->caches[i].x = 0;
        data->caches[i].y = 0;
        data->caches[i].z = 0;
    }

    event->value = 0;
    event->sync = false;

    /* smooth with recent */
    if (config->smooth_len > 0) {
        data->smooth_x[data->smooth_idx] = data->x;
        data->smooth_y[data->smooth_idx] = data->y;
        data->smooth_z[data->smooth_idx] = data->z;
        data->smooth_rx[data->smooth_idx] = data->rx;
        data->smooth_ry[data->smooth_idx] = data->ry;
        data->smooth_rz[data->smooth_idx] = data->rz;
        data->smooth_filled = MAX(data->smooth_filled, data->smooth_idx + 1);
        float sum_x = 0, sum_y = 0, sum_z = 0;
        float sum_rx = 0, sum_ry = 0, sum_rz = 0;
        float sum_w = 0;
        for (int i = 0; i < data->smooth_filled; i++) {

            /* variants of weight distribution by index distance
            int d = abs(i - data->smooth_idx);
            float w = 0.5f + 0.5f * cosf(M_PI * d / (float)(data->smooth_filled));
            float w = 0.65f + 0.35f * cosf(M_PI * d / (float)(data->smooth_filled));
            float w = 0.75f + 0.25f * cosf(M_PI * d / (float)(data->smooth_filled));
            float w = 1.0f;
            */
            int d = abs(i - data->smooth_idx);
            float w = 0.75f + 0.25f * cosf(M_PI * d / (float)(data->smooth_filled));

            sum_x += w * (float)data->smooth_x[i];
            sum_y += w * (float)data->smooth_y[i];
            sum_z += w * (float)data->smooth_z[i];
            sum_rx += w * (float)data->smooth_rx[i];
            sum_ry += w * (float)data->smooth_ry[i];
            sum_rz += w * (float)data->smooth_rz[i];
            sum_w += w;
        }
        data->x = (int16_t)roundf(sum_x / sum_w);
        data->y = (int16_t)roundf(sum_y / sum_w);
        data->z = (int16_t)roundf(sum_z / sum_w);
        data->rx = (int16_t)roundf(sum_rx / sum_w);
        data->ry = (int16_t)roundf(sum_ry / sum_w);
        data->rz = (int16_t)roundf(sum_rz / sum_w);
        data->smooth_idx = (data->smooth_idx + 1) % config->smooth_len;
    }

    /* apply deadzones */
    bool x_in_dz = abs(data->x) < (int)config->rpt_dzn_x;
    bool y_in_dz = abs(data->y) < (int)config->rpt_dzn_y;
    bool z_in_dz = abs(data->z) < (int)config->rpt_dzn_z;
    bool rx_in_dz = abs(data->rx) < (int)config->rpt_dzn_rx;
    bool ry_in_dz = abs(data->ry) < (int)config->rpt_dzn_ry;
    bool rz_in_dz = abs(data->rz) < (int)config->rpt_dzn_rz;
    bool in_deadzone = x_in_dz && y_in_dz && z_in_dz && rx_in_dz && ry_in_dz && rz_in_dz;
    // LOG_DBG("%d %d %d %d %d %d", 
    //     abs(data->x), abs(data->y), abs(data->z),
    //     abs(data->rx), abs(data->ry), abs(data->rz));
    if (in_deadzone) { 
         if (!data->neutral_pending && !data->in_neutral_state) {
            k_work_schedule(&data->neutral_work, K_MSEC(0));
            data->neutral_pending = true;
        }
        return ZMK_INPUT_PROC_STOP;
    }

    int64_t now = k_uptime_get();

    /* Report accumulated values at configured interval */
    if (now - data->last_rpt_time >= config->sync_report_ms) {
        bool have_pos = data->x != 0 || data->y != 0 || data->z != 0;
        bool have_rot = data->rx != 0 || data->ry != 0 || data->rz != 0;
        
        if (have_pos || have_rot) {
            data->last_rpt_time = now;
            
            /* Report translation (X, Y, Z) - sync on last axis if no rotation */
            if (data->x != 0) {
                input_report(dev, INPUT_EV_REL, INPUT_REL_X, data->x,
                           data->y == 0 && data->z == 0 && !have_rot, K_NO_WAIT);
            }
            if (data->y != 0) {
                input_report(dev, INPUT_EV_REL, INPUT_REL_Y, data->y,
                           data->z == 0 && !have_rot, K_NO_WAIT);
            }
            if (data->z != 0) {
                input_report(dev, INPUT_EV_REL, INPUT_REL_Z, data->z,
                           !have_rot, K_NO_WAIT);
            }

            /* Report rotation (RX, RY, RZ) - sync on RZ */
            if (data->rx != 0) {
                input_report(dev, INPUT_EV_REL, INPUT_REL_RX, data->rx,
                           data->ry == 0 && data->rz == 0, K_NO_WAIT);
            }
            if (data->ry != 0) {
                input_report(dev, INPUT_EV_REL, INPUT_REL_RY, data->ry,
                           data->rz == 0, K_NO_WAIT);
            }
            if (data->rz != 0) {
                input_report(dev, INPUT_EV_REL, INPUT_REL_RZ, data->rz,
                           true, K_NO_WAIT);
            }

            /* Clear reported values */
            data->x = data->y = data->z = 0;
            data->rx = data->ry = data->rz = 0;
        }
    }

    return ZMK_INPUT_PROC_STOP;
}

/*
 * Work handler for sending neutral (zero) report.
 * Called when the neutral timeout expires - all sensors have been idle.
 */
static void neutral_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct zip_trixer_data *data = CONTAINER_OF(dwork, struct zip_trixer_data, neutral_work);
    const struct device *dev = data->dev;
    const struct zip_trixer_config *config = dev->config;

    /* Mark neutral as no longer pending but we are now in neutral state */
    data->neutral_pending = false;
    data->in_neutral_state = true;

    /* Send zero on all 6 axes to HID joystick */
    input_report(dev, INPUT_EV_REL, INPUT_REL_X, 0, false, K_NO_WAIT);
    input_report(dev, INPUT_EV_REL, INPUT_REL_Y, 0, false, K_NO_WAIT);
    input_report(dev, INPUT_EV_REL, INPUT_REL_Z, 0, false, K_NO_WAIT);
    input_report(dev, INPUT_EV_REL, INPUT_REL_RX, 0, false, K_NO_WAIT);
    input_report(dev, INPUT_EV_REL, INPUT_REL_RY, 0, false, K_NO_WAIT);
    input_report(dev, INPUT_EV_REL, INPUT_REL_RZ, 0, true, K_NO_WAIT);
    // LOG_DBG("NEUTRAL report sent (all axes zero)");

    /* reset all smoothing on neutral */
    if (config->smooth_len > 0) {
        for (int i = 0; i < config->smooth_len; i++) {
            data->smooth_x[i] = 0;
            data->smooth_y[i] = 0;
            data->smooth_z[i] = 0;
            data->smooth_rx[i] = 0;
            data->smooth_ry[i] = 0;
            data->smooth_rz[i] = 0;
        }
        data->smooth_filled = 0;
        data->smooth_idx = 0;
    }
}

static struct zmk_input_processor_driver_api trixer_driver_api = {
    .handle_event = trixer_handle_event,
};

static int trixer_init(const struct device *dev)
{
    struct zip_trixer_data *data = dev->data;
    const struct zip_trixer_config *config = dev->config;
    data->dev = dev;

    /* Initialize work item for neutral timeout */
    k_work_init_delayable(&data->neutral_work, neutral_work_handler);
    data->neutral_pending = false;
    data->in_neutral_state = false;

    /* Initialize smoothing history */
    if (config->smooth_len > 0) {
        data->smooth_x = malloc(config->smooth_len * sizeof(int16_t));
        data->smooth_y = malloc(config->smooth_len * sizeof(int16_t));
        data->smooth_z = malloc(config->smooth_len * sizeof(int16_t));
        data->smooth_rx = malloc(config->smooth_len * sizeof(int16_t));
        data->smooth_ry = malloc(config->smooth_len * sizeof(int16_t));
        data->smooth_rz = malloc(config->smooth_len * sizeof(int16_t));
        if (!data->smooth_x || !data->smooth_y || !data->smooth_z
        || !data->smooth_rx || !data->smooth_ry || !data->smooth_rz) {
            LOG_ERR("Failed to allocate smoothing history slots");
            return -ENOMEM;
        }
        for (int i = 0; i < config->smooth_len; i++) {
            data->smooth_x[i] = 0;
            data->smooth_y[i] = 0;
            data->smooth_z[i] = 0;
            data->smooth_rx[i] = 0;
            data->smooth_ry[i] = 0;
            data->smooth_rz[i] = 0;
        }
        data->smooth_filled = 0;
        data->smooth_idx = 0;
    }

    /* Clear sensor caches */
    for (int i = 0; i < NUM_SENSORS; i++) {
        data->caches[i].x = 0;
        data->caches[i].y = 0;
        data->caches[i].z = 0;
        data->caches_valid[i] = false;
    }

    return 0;
}

#define TRIXER_INST(n)                                                                         \
    static struct zip_trixer_data data_##n = {};                                               \
    static struct zip_trixer_config config_##n = {                                             \
        .sync_report_ms = DT_INST_PROP(n, sync_report_ms),                                     \
        .radius_um = DT_INST_PROP_OR(n, radius_um, 18500),                                     \
        .xy_sensitivity_num = DT_INST_PROP_OR(n, xy_sensitivity_num, 1),                       \
        .xy_sensitivity_denom = DT_INST_PROP_OR(n, xy_sensitivity_denom, 1),                   \
        .z_sensitivity_num = DT_INST_PROP_OR(n, z_sensitivity_num, 1),                         \
        .z_sensitivity_denom = DT_INST_PROP_OR(n, z_sensitivity_denom, 1),                     \
        .pitch_scale_num = DT_INST_PROP_OR(n, pitch_scale_num, 1),                             \
        .pitch_scale_denom = DT_INST_PROP_OR(n, pitch_scale_denom, 1),                         \
        .roll_scale_num = DT_INST_PROP_OR(n, roll_scale_num, 1),                               \
        .roll_scale_denom = DT_INST_PROP_OR(n, roll_scale_denom, 1),                           \
        .yaw_scale_num = DT_INST_PROP_OR(n, yaw_scale_num, 1),                                 \
        .yaw_scale_denom = DT_INST_PROP_OR(n, yaw_scale_denom, 1),                             \
        .yaw_comp_x_num = DT_INST_PROP_OR(n, yaw_comp_x_num, 0),                               \
        .yaw_comp_x_denom = DT_INST_PROP_OR(n, yaw_comp_x_denom, 1),                           \
        .neutral_timeout_ms = DT_INST_PROP_OR(n, neutral_timeout_ms, 50),                      \
        .smooth_len = DT_INST_PROP(n, smooth_len),                                             \
        .rpt_dzn_x = DT_INST_PROP(n, rpt_dzn_x),                                               \
        .rpt_dzn_y = DT_INST_PROP(n, rpt_dzn_y),                                               \
        .rpt_dzn_z = DT_INST_PROP(n, rpt_dzn_z),                                               \
        .rpt_dzn_rx = DT_INST_PROP(n, rpt_dzn_rx),                                             \
        .rpt_dzn_ry = DT_INST_PROP(n, rpt_dzn_ry),                                             \
        .rpt_dzn_rz = DT_INST_PROP(n, rpt_dzn_rz),                                             \
    };                                                                                         \
    DEVICE_DT_INST_DEFINE(n, &trixer_init, NULL, &data_##n, &config_##n, POST_KERNEL,          \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &trixer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TRIXER_INST)
