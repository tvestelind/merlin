FROM centos:7

# Building tools
RUN yum install --assumeyes \
    autoconf \
    automake \
    check-devel \
    gcc \
    gdb \
    git \
    glib2-devel \
    gperf \
    help2man \
    libdbi \
    libdbi-dbd-mysql \
    libdbi-devel \
    libdbi-drivers \
    libtool \
    make \
    mariadb \
    mariadb-devel \
    mariadb-libs \
    mariadb-server \
    monitor-livestatus \
    ruby-devel \
    rubygems

RUN gem install \
    cucumber \
    multi_json \
    mysql2 \
    sequel

RUN yum upgrade --assumeyes && yum clean all

RUN git clone https://github.com/naemon/naemon-core.git
WORKDIR naemon-core
ARG NAEMON_BRANCH=origin/master
RUN git fetch -a && git checkout $NAEMON_BRANCH
RUN ./autogen.sh && make -j install
