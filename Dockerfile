# syntax=docker/dockerfile:1
FROM ubuntu:26.04

ARG ZEPHYR_VERSION=v4.5.0
ARG ZEPHYR_SDK_VERSION=1.0.1

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates cmake device-tree-compiler file g++ gcc git gperf \
    make ninja-build python3-dev python3-venv wget xz-utils \
    && rm -rf /var/lib/apt/lists/*

ENV PATH="/opt/venv/bin:${PATH}" \
    ZEPHYR_BASE=/opt/zephyr-workspace/zephyr \
    CMAKE_PREFIX_PATH=/opt/zephyr-workspace/zephyr/share/zephyr-package:/opt/zephyr-workspace/zephyr/share/sysbuild-package \
    ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
    ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk \
    HOME=/tmp/ergodox-home \
    XDG_CACHE_HOME=/tmp/ergodox-cache \
    PYTHONDONTWRITEBYTECODE=1

RUN python3 -m venv /opt/venv \
    && pip install --no-cache-dir west==1.5.0 \
    && git init /opt/zephyr-workspace/zephyr \
    && git -C /opt/zephyr-workspace/zephyr remote add origin https://github.com/zephyrproject-rtos/zephyr \
    && git -C /opt/zephyr-workspace/zephyr fetch --depth 1 origin "${ZEPHYR_VERSION}" \
    && git -C /opt/zephyr-workspace/zephyr checkout --detach FETCH_HEAD \
    && west init -l /opt/zephyr-workspace/zephyr

WORKDIR /opt/zephyr-workspace
# Only the modules needed for this RP2350 ARM firmware, at manifest-pinned revisions.
RUN west update --narrow cmsis cmsis_6 hal_rpi_pico picolibc \
    && pip install --no-cache-dir -r zephyr/scripts/requirements-base.txt \
    && west sdk install --version "${ZEPHYR_SDK_VERSION}" \
        --install-dir /opt/zephyr-sdk --gnu-toolchains arm-zephyr-eabi --no-hosttools \
    && find /opt/zephyr-workspace -type d -name .git -print | \
        while IFS= read -r git_dir; do \
            git config --system --add safe.directory "${git_dir%/.git}"; \
        done

# RTT logging uses the SEGGER module at the Zephyr manifest-pinned revision.
RUN west update --narrow segger

# Zephyr may fall back to a source-tree cache during sysbuild configuration.
# Keep that fallback writable when Docker runs with an arbitrary host UID.
RUN rm -rf /tmp/ergodox-home /tmp/ergodox-cache \
    && ln -s /tmp/ergodox-cache/zephyr /opt/zephyr-workspace/zephyr/.cache

COPY docker/entrypoint.sh /usr/local/bin/ergodox-build
RUN chmod +x /usr/local/bin/ergodox-build
ENTRYPOINT ["/usr/local/bin/ergodox-build"]
CMD ["build"]
