FROM centos:7

# Building tools
RUN yum install --assumeyes \
    autoconf \
    automake \
    check-devel \
    gcc \
    gcc-c++ \
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
    cucumber:1.3.6 \
    multi_json \
    mysql2 \
    rspec \
    sequel \
    syntax

RUN yum upgrade --assumeyes && yum clean all

ENV NAEMON_DIR /tmp/naemon_repo
RUN git clone https://github.com/naemon/naemon-core.git $NAEMON_DIR
WORKDIR $NAEMON_DIR
ARG NAEMON_BRANCH=origin/master
RUN git fetch -a && git checkout $NAEMON_BRANCH
RUN autoreconf -if  && ./configure --prefix=/usr --libdir=/lib64 && make -j install

ENV LIVESTATUS_DIR /tmp/livestatus_repo
RUN git clone https://github.com/naemon/naemon-livestatus.git $LIVESTATUS_DIR
WORKDIR $LIVESTATUS_DIR
ARG LIVESTATUS_BRANCH=origin/master
RUN git fetch -a && git checkout $LIVESTATUS_BRANCH
RUN autoreconf -if  && ./configure --prefix=/usr --libdir=/lib64 && make -j install

ENV MERLIN_DIR /tmp/merlin_repo
COPY . $MERLIN_DIR
WORKDIR $MERLIN_DIR
RUN ./autogen.sh && make -j all cukemerlin
