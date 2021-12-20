FROM matthewfeickert/docker-python3-ubuntu:latest
USER root
COPY sources.list instantclient-basic-linux.x64-12.2.0.1.0.zip instantclient-sqlplus-linux.x64-12.2.0.1.0.zip requirements-a.txt ./
COPY ./dmdb/ ./

ADD jdk-8u301-linux-x64.tar.gz  /usr/local/java/

ENV DM_HOME /data/publish/python/dmdbms
ENV ORACLE_HOME /opt/oracle/instantclient_12_2
ENV PATH $PATH:${ORACLE_HOME}
ENV LD_LIBRARY_PATH $LD_LIBRARY_PATH:/opt/oracle/instantclient_12_2:/data/publish/python/dmdbms/
ENV JAVA_HOME /usr/local/java/jdk
ENV JRE_HOME ${JAVA_HOME}/jre
ENV CLASSPATH .:${JAVA_HOME}/lib:${JRE_HOME}/lib
ENV PATH ${JAVA_HOME}/bin:$PATH
	
RUN set -ex \
    && sudo mkdir /opt/oracle \
    && sudo cp sources.list /etc/apt/sources.list \
    && sudo mkdir -p /data/publish/python/dmdbms \
    && sudo cp -r ./dpi/* /data/publish/python/dmdbms/ \
    && cd ./python/dmPython_C/dmPython \
    && rm -rf sources.list* \
    && sudo apt update -y \
    && sudo apt upgrade -y \
    && sudo apt install -y exiv2 python3-dev libexiv2-dev libboost-python-dev \
    && sudo apt install -y libmediainfo-dev \
    && sudo apt install -y python3-pip libkrb5-dev openssh-server ffmpeg \
    && sudo apt install -y zip \
    && sudo apt-get install libaio1 libaio-dev \
    	# python env install
    && mkdir -p ~/.pip \
    && echo '[global]' > ~/.pip/pip.conf \
    && echo 'index-url = https://pypi.tuna.tsinghua.edu.cn/simple' >> ~/.pip/pip.conf \
    && echo 'trusted-host = pypi.tuna.tsinghua.edu.cn' >> ~/.pip/pip.conf \
    && echo 'timeout = 120' >> ~/.pip/pip.conf \
    && pip install --upgrade pip \
    && pip install wheel \
	# dmdb drive config
    && python setup.py install \
    && sudo rm -rf ~/data/dpi \
    && sudo rm -rf ~/data/python \
    # && sudo python -m pip install --upgrade pip \
	# python dependency  install 
    && cd /home/docker/data \
    && pip install -r  requirements-a.txt \
    && sudo rm -rf requirements-a.txt \
    	# oracle driver
    && sudo unzip instantclient-basic-linux.x64-12.2.0.1.0.zip  -d /opt/oracle \
    && sudo unzip instantclient-sqlplus-linux.x64-12.2.0.1.0.zip  -d /opt/oracle/ \
    && sudo rm -rf instantclient-basic-linux.x64-12.2.0.1.0.zip \
    && sudo rm -rf instantclient-sqlplus-linux.x64-12.2.0.1.0.zip \
    && sudo sh -c "echo /opt/oracle/instantclient_12_2 > /etc/ld.so.conf.d/oracle-instantclient.conf" \
    && sudo ldconfig \
    # java config
    && sudo ln -s /usr/local/java/jdk1.8.0_301 /usr/local/java/jdk

COPY ./django_dmPython ./

RUN set -ex \
   # && echo $(pwd) \
   # && ln -s /home/docker/data/django_dmPython django_dmPython \
   # && cd /home/docker/data/django_dmPython \
    && python setup.py install

RUN set -ex \
	&& cd ~ \
	&& rm -rf .pip \
	&& rm -rf data

ADD sshd_config /etc/ssh/sshd_config
	


