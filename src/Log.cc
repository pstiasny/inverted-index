#include "inverted_index.h"


Log::Log(string path) {
        file = fopen(path.c_str(), "a+");
        if (!file)
            throw IOError(string("could not open log: ") + strerror(errno));
        if (0 != fseek(file, 0, SEEK_SET))
            throw IOError(string("could not seek in log: ") + strerror(errno));
    }

Log::~Log() {
        close();
    }

void Log::writeOp(const AddOp &o) {
    assert(o.seqid > last_seqid);

    if (0 != fseek(file, 0, SEEK_END))
        throw IOError(string("could not seek in log: ") + strerror(errno));

    int id_size = o.e->id.size()
      , content_size =o.e->content.size();
    lwrite(&o.seqid, sizeof(o.seqid), 1);
    lwrite(&id_size, sizeof(id_size), 1);
    lwrite(&content_size, sizeof(content_size), 1);
    lwrite(o.e->id.c_str(), o.e->id.size(), 1);
    lwrite(o.e->content.c_str(), o.e->content.size(), 1);
    lflush();

    last_seqid = o.seqid;
}

shared_ptr<AddOp> Log::readOp() {
    auto op = make_shared<AddOp>();

    int r;
    int seqid, id_size, content_size;
    if (!ltry_read(&(op->seqid), sizeof(seqid), 1))
        return nullptr;
    lread(&id_size, sizeof(id_size), 1);
    lread(&content_size, sizeof(content_size), 1);

    assert(op->seqid > last_seqid);
    last_seqid = op->seqid;

    char id[id_size], content[content_size];
    lread(id, sizeof(char), id_size);
    lread(content, sizeof(char), content_size);
    op->e = make_shared<Entity>(string(id, id_size), string(content, content_size));

    return op;
}

void Log::close() {
    fclose(file);
}

int Log::getNextSeqid() {
    return last_seqid + 1;
}

void Log::lread(void *ptr, size_t size, size_t n) {
    if (n != fread(ptr, size, n, file)) {
        if (feof(file)) {
            throw IOError("unexepected end of log");
        } else {
            throw IOError("error when reading log");
        }
    }
}

bool Log::ltry_read(void *ptr, size_t size, size_t n) {
    if (n != fread(ptr, size, n, file)) {
        if (feof(file)) {
            return false;
        } else {
            throw IOError("error when reading log");
        }
    }
    return true;
}

void Log::lwrite(const void *ptr, size_t size, size_t nitems) {
    if (nitems != fwrite(ptr, size, nitems, file))
        throw IOError("could not write to log");
}

void Log::lflush() {
    fflush(file);
}
