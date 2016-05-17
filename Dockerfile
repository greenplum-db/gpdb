FROM pivotaldata/gpdb-devel

WORKDIR /workspace

ADD . gpdb/

WORKDIR gpdb

RUN ./configure --with-python --with-perl --enable-mapreduce --with-libxml --prefix=/usr/local/gpdb
RUN time make -j4
RUN make install

RUN chown -R gpadmin:gpadmin /workspace/gpdb
RUN chown -R gpadmin:gpadmin /usr/local/gpdb
