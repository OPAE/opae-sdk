FROM fedora:32
RUN dnf install -y python3 python3-pip python3-devel python3-pybind11 cmake make libuuid-devel json-c-devel gcc clang gcc-c++ hwloc-devel tbb-devel rpm-build rpmdevtools git
RUN dnf install -y libedit-devel
RUN dnf install -y libudev-devel
RUN dnf install -y libcap-devel
RUN python3 -m pip install setuptools --upgrade
RUN python3 -m pip install python-pkcs11 pyyaml jsonschema
WORKDIR /root
COPY scripts/build-rpms.sh /scripts/build-rpms.sh
COPY scripts/test-rpms.sh /scripts/test-rpms.sh
ENTRYPOINT [ "/scripts/build-rpms.sh" ]

