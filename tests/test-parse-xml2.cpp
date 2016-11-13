#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cassert>

#include <boost/make_shared.hpp>

#include "osmtypes.hpp"
#include "osmdata.hpp"
#include "parse-xml2.hpp"
#include "output.hpp"
#include "options.hpp"

void exit_nicely()
{
    fprintf(stderr, "Error occurred, cleaning up\n");
    exit(1);
}

struct test_middle_t : public middle_t {
    virtual ~test_middle_t() {}

    int start(const options_t *out_options_) { return 0; }
    void stop(void) { }
    void cleanup(void) { }
    void analyze(void) { }
    void end(void) { }
    void commit(void) { }

    int nodes_set(osmid_t id, double lat, double lon, const taglist_t &tags) { return 0; }
    int nodes_get_list(nodelist_t &out, const idlist_t nds) const { return 0; }

    int ways_set(osmid_t id, const idlist_t &nds, const taglist_t &tags) { return 0; }
    int ways_get(osmid_t id, taglist_t &tags, nodelist_t &nodes) const { return 0; }
    int ways_get_list(const idlist_t &ids, idlist_t &way_ids,
                              std::vector<taglist_t> &tags,
                              std::vector<nodelist_t> &nodes) const { return 0; }

    int relations_set(osmid_t id, const memberlist_t &members, const taglist_t &tags) { return 0; }
    int relations_get(osmid_t id, memberlist_t &members, taglist_t &tags) const { return 0; }

    void iterate_ways(pending_processor& pf) { }
    void iterate_relations(pending_processor& pf) { }

    virtual size_t pending_count() const { return 0; }

    std::vector<osmid_t> relations_using_way(osmid_t way_id) const { return std::vector<osmid_t>(); }

    virtual boost::shared_ptr<const middle_query_t> get_instance() const {return boost::shared_ptr<const middle_query_t>();}
};

struct test_output_t : public output_t {
    uint64_t sum_ids, num_nodes, num_ways, num_relations, num_nds, num_members;

    explicit test_output_t(const options_t &options_)
        : output_t(NULL, options_), sum_ids(0), num_nodes(0), num_ways(0), num_relations(0),
          num_nds(0), num_members(0) {
    }

    explicit test_output_t(const test_output_t &other)
        : output_t(other.m_mid, other.m_options), sum_ids(0), num_nodes(0), num_ways(0), num_relations(0),
          num_nds(0), num_members(0) {
    }

    virtual ~test_output_t() {
    }

    boost::shared_ptr<output_t> clone(const middle_query_t *cloned_middle) const{
        test_output_t *clone = new test_output_t(*this);
        clone->m_mid = cloned_middle;
        return boost::shared_ptr<output_t>(clone);
    }

    int node_add(osmid_t id, double lat, double lon, const taglist_t &tags) {
        assert(id > 0);
        sum_ids += id;
        num_nodes += 1;
        return 0;
    }

    int way_add(osmid_t id, const idlist_t &nds, const taglist_t &tags) {
        assert(id > 0);
        sum_ids += id;
        num_ways += 1;
        assert(nds.size() >= 0);
        num_nds += uint64_t(nds.size());
        return 0;
    }

    int relation_add(osmid_t id, const memberlist_t &members, const taglist_t &tags) {
        assert(id > 0);
        sum_ids += id;
        num_relations += 1;
        assert(members.size() >= 0);
        num_members += uint64_t(members.size());
        return 0;
    }

    int start() { return 0; }
    int connect(int startTransaction) { return 0; }
    void stop() { }
    void commit() { }
    void cleanup(void) { }
    void close(int stopTransaction) { }

    void enqueue_ways(pending_queue_t &job_queue, osmid_t id, size_t output_id, size_t& added) { }
    int pending_way(osmid_t id, int exists) { return 0; }

    void enqueue_relations(pending_queue_t &job_queue, osmid_t id, size_t output_id, size_t& added) { }
    int pending_relation(osmid_t id, int exists) { return 0; }

    int node_modify(osmid_t id, double lat, double lon, const taglist_t &tags) { return 0; }
    int way_modify(osmid_t id, const idlist_t &nds, const taglist_t &tags) { return 0; }
    int relation_modify(osmid_t id, const memberlist_t &members, const taglist_t &tags) { return 0; }

    int node_delete(osmid_t id) { return 0; }
    int way_delete(osmid_t id) { return 0; }
    int relation_delete(osmid_t id) { return 0; }

};


void assert_equal(uint64_t actual, uint64_t expected) {
  if (actual != expected) {
    std::cerr << "Expected " << expected << ", but got " << actual << ".\n";
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  char *srcdir = getenv("srcdir");

  if (srcdir == NULL) {
    std::cerr << "$srcdir not set!\n";
    return 1;
  }

  std::string inputfile = std::string(srcdir) + std::string("/tests/test_multipolygon.osm");

  options_t options;
  boost::shared_ptr<reprojection> projection(new reprojection(PROJ_SPHERE_MERC));
  options.projection = projection;

  boost::shared_ptr<test_output_t> out_test(new test_output_t(options));
  osmdata_t osmdata(boost::make_shared<test_middle_t>(), out_test);

  parse_xml2_t parser(0, false, projection, 0, 0, 0, 0);

  int ret = parser.streamFile(inputfile.c_str(), 0, &osmdata);
  if (ret != 0) {
    return ret;
  }

  assert_equal(out_test->sum_ids,       73514L);
  assert_equal(out_test->num_nodes,       353L);
  assert_equal(out_test->num_ways,        140L);
  assert_equal(out_test->num_relations,    40L);
  assert_equal(out_test->num_nds,         495L);
  assert_equal(out_test->num_members,     146L);

  return 0;
}
