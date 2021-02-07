#include "phy.h"


TreePrinter::TreePrinter(ostream &out, bool print_inner_key_data) : out(out) {
    this->print_inner_key_data = print_inner_key_data;
};

void TreePrinter::enterNode(TreeCursor &tc, NodeRef nr) {
    out << prefix << "enter inner node " << nr.index << endl;
    prefix += "  ";
};

void TreePrinter::enterLeaf(TreeCursor &tc, NodeRef nr) {
    out << prefix << "enter leaf node " << nr.index << endl;
    prefix += "  ";
};

void TreePrinter::enterNodeItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) {
    out << prefix << "inner node key #" << item_idx << ": " << tc.key() << endl;
    if (print_inner_key_data)
        out << prefix << "  internal key data: " << nr->copy_inner_key_data(item_idx) << endl;
};

void TreePrinter::enterLeafItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) {
    out << prefix << "leaf node key #" << item_idx << ": " << tc.key() << endl;
    if (print_inner_key_data)
        out << prefix << "  internal key data: " << nr->copy_inner_key_data(item_idx)
            << " offset: " << item->inner_key_data_offset
            << " length: " << item->inner_key_length << endl;
    out << prefix << "  content: " << tc.content() << endl;
};

void TreePrinter::exitNode(TreeCursor &tc, NodeRef nr) {
    prefix.pop_back();
    prefix.pop_back();
    out << prefix << "exit inner node " << nr.index << endl;
};

void TreePrinter::exitLeaf(TreeCursor &tc, NodeRef nr) {
    prefix.pop_back();
    prefix.pop_back();
    out << prefix << "exit leaf node " << nr.index << endl;
};
