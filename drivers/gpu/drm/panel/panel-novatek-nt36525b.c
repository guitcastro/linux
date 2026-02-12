// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Guilherme Castro
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved.

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct nt36525b_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
};

static inline
struct nt36525b_panel *to_nt36525b_panel(struct drm_panel *panel)
{
	return container_of(panel, struct nt36525b_panel, panel);
}

static void nt36525b_reset(struct nt36525b_panel *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
}

static int nt36525b_on(struct nt36525b_panel *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x02, 0x03, 0x05, 0x06, 0x08, 0x0d, 0x0d,
				     0x0e, 0x0f, 0x10, 0x12, 0x40, 0x38, 0x24,
				     0x24, 0x2c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2,
				     0x02, 0x03, 0x05, 0x06, 0x08, 0x0d, 0x0d,
				     0x0e, 0x0f, 0x10, 0x12, 0x12, 0x38, 0x24,
				     0x24, 0x2c);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x23);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x00, 0x68);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x05, 0x12);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x06, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x07, 0x20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x08, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x09, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x12, 0xab);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x15, 0x7b);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x16, 0x0a);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x30, 0xff);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x31, 0xfe);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x32, 0xfc);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x33, 0xfa);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x34, 0xf8);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x35, 0xf6);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x36, 0xf4);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x37, 0xf2);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x38, 0xf1);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x39, 0xf0);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x3a, 0xef);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x3b, 0xed);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x3d, 0xeb);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x3f, 0xe9);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x40, 0xe7);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x41, 0xe5);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x45, 0xff);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x46, 0xfc);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x47, 0xf6);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x48, 0xec);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x49, 0xdf);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4a, 0xcd);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4b, 0xc0);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4c, 0xb3);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4d, 0xa6);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4e, 0x98);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4f, 0x8f);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x50, 0x86);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x51, 0x7d);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x52, 0x74);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x53, 0x6d);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x54, 0x66);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x58, 0xff);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x59, 0xfd);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x5a, 0xf9);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x5b, 0xf2);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x5c, 0xec);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x5d, 0xe6);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x5e, 0xde);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x5f, 0xd9);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x60, 0xd2);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x61, 0xcc);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x62, 0xc7);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x63, 0xc3);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x64, 0xbe);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x65, 0xb9);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x66, 0xb5);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x67, 0xb3);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x22);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x01, 0x04);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x02, 0x08);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x03, 0x0c);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x04, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x05, 0x14);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x06, 0x18);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x07, 0x20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x08, 0x24);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x09, 0x28);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x0a, 0x30);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x0b, 0x38);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x0c, 0x38);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x0d, 0x30);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x0e, 0x28);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x0f, 0x20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x10, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x11, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x12, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x13, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x1a, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x1b, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x1c, 0x04);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x1d, 0x08);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x1e, 0x0c);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x1f, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x20, 0x14);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x21, 0x18);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x22, 0x20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x23, 0x18);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x24, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x25, 0x08);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x26, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x27, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x28, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x29, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x2a, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x2b, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x2f, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x30, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x31, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x32, 0x9c);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x33, 0x94);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x34, 0x94);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x35, 0x1e);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x36, 0x14);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x37, 0x1e);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x38, 0x1e);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x39, 0x14);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x3a, 0x0a);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x3b, 0x0c);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x3f, 0x0e);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x40, 0x15);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x41, 0x1b);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x42, 0x20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x43, 0x26);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x44, 0x1e);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x45, 0x17);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x46, 0x0f);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x47, 0x08);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x48, 0x0a);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x49, 0x12);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4a, 0x23);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4b, 0x28);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4c, 0x9c);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x4d, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x53, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x54, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x55, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x56, 0x76);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x68, 0x76);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa2, 0x20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x26);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x45, 0x0e);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x55, 0x80);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xba, 0x03);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x51, 0x0f, 0xff);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x53, 0x2c);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x29, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x11, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 100);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x25);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x24, 0xa7);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x46, 0x13);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x47, 0x94);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x26);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x31, 0x11);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x32, 0x18);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x36, 0x67);

	return dsi_ctx.accum_err;
}

static int nt36525b_off(struct nt36525b_panel *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xff, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xfb, 0x01);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x28, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 20);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x10, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int nt36525b_prepare(struct drm_panel *panel)
{
	struct nt36525b_panel *ctx = to_nt36525b_panel(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	nt36525b_reset(ctx);

	ret = nt36525b_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	return 0;
}

static int nt36525b_unprepare(struct drm_panel *panel)
{
	struct nt36525b_panel *ctx = to_nt36525b_panel(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = nt36525b_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	return 0;
}

static const struct drm_display_mode nt36525b_mode = {
	.clock = (720 + 101 + 8 + 100) * (1520 + 10 + 2 + 8) * 60 / 1000,
	.hdisplay = 720,
	.hsync_start = 720 + 101,
	.hsync_end = 720 + 101 + 8,
	.htotal = 720 + 101 + 8 + 100,
	.vdisplay = 1520,
	.vsync_start = 1520 + 10,
	.vsync_end = 1520 + 10 + 2,
	.vtotal = 1520 + 10 + 2 + 8,
	.width_mm = 68,
	.height_mm = 143,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int nt36525b_get_modes(struct drm_panel *panel,
			      struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &nt36525b_mode);
}

static const struct drm_panel_funcs nt36525b_panel_funcs = {
	.prepare = nt36525b_prepare,
	.unprepare = nt36525b_unprepare,
	.get_modes = nt36525b_get_modes,
};

static int nt36525b_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct nt36525b_panel *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct nt36525b_panel, panel,
				   &nt36525b_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void nt36525b_remove(struct mipi_dsi_device *dsi)
{
	struct nt36525b_panel *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id nt36525b_of_match[] = {
	{ .compatible = "novatek,nt36525b-olive" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nt36525b_of_match);

static struct mipi_dsi_driver nt36525b_driver = {
	.probe = nt36525b_probe,
	.remove = nt36525b_remove,
	.driver = {
		.name = "panel-novatek-nt36525b",
		.of_match_table = nt36525b_of_match,
	},
};
module_mipi_dsi_driver(nt36525b_driver);

MODULE_DESCRIPTION("DRM driver for Novatek NT36525B DSI video mode panel");
MODULE_LICENSE("GPL");
