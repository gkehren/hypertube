# Licensing and Compliance Guide

This document outlines the licensing architecture for Hypertube, compliance rules for binary releases, and third-party attribution requirements.

## Overview

Hypertube is distributed under the **GNU General Public License v3.0 (GPLv3)**.

Binary distributions and installers include:
- The executable (`hypertube`)
- Root license file (`LICENSE`)
- Third-party notices (`THIRD_PARTY_NOTICES.md`)
- Default seed configuration directory (`config/`)

## Dependency Licenses Summary

| Subsystem / Dependency | Role | License |
| :--- | :--- | :--- |
| **Slint** | GUI Toolkit | GPLv3 / Commercial |
| **libtorrent-rasterbar** | BitTorrent Engine | BSD-3-Clause |
| **cURL** | Search HTTP Requests | curl (MIT-style) |
| **nlohmann/json** | State Persistence | MIT |
| **GoogleTest** | Unit Testing | BSD-3-Clause |

## Packaging Rules

All CPack ZIP packages and portable release directories MUST include `LICENSE` and `THIRD_PARTY_NOTICES.md` alongside the binary executable.
