FROM microros/micro-ros-agent:humble

RUN apt-get update && apt-get install -y --no-install-recommends \
    ros-humble-rmw-cyclonedds-cpp \
    && rm -rf /var/lib/apt/lists/*

ENV RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
ENV ROS_DOMAIN_ID=0

CMD ["serial", "--dev", "/dev/ttyUSB0", "-b", "115200", "-v4"]