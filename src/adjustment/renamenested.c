#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "ccngen/trav_data.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "util/funtable.h"
#include <stdio.h>
#include <string.h>

void RNFinit(void) { }
void RNFfini(void) { }

node_st *RNFprogram(node_st *node) {
    struct data_rnf *data = DATA_RNF_GET();
    data->name_prefix = NULL;
    TRAVchildren(node);
    return node;
}

node_st *RNFfundef(node_st *node) {
    struct data_rnf *data = DATA_RNF_GET();
    node_st *header = FUNDEF_HEADER(node);
    char *old_name = FUNHEADER_NAME(header);
    char *new_full_name = NULL;

    if (data->name_prefix != NULL) {
        new_full_name = STRcat(data->name_prefix, old_name);
        MEMfree(FUNHEADER_NAME(header));
        FUNHEADER_NAME(header) = STRcpy(new_full_name);
    } else {
        new_full_name = STRcpy(old_name);
    }

    // Prepare prefix for children
    char *saved_prefix = data->name_prefix;
    data->name_prefix = STRcat(new_full_name, "*");
    
    TRAVchildren(node);

    // Restore
    MEMfree(data->name_prefix);
    data->name_prefix = saved_prefix;
    MEMfree(new_full_name);

    return node;
}

node_st *RNFfuncall(node_st *node) {
    Function *fun = FUNCALL_FUNPTR(node);
    if (fun != NULL && fun->fundef_node != NULL) {
        node_st *source_node = (node_st *)fun->fundef_node;
        node_st *source_header = NULL;
        if (NODE_TYPE(source_node) == NT_FUNDEF) {
            source_header = FUNDEF_HEADER(source_node);
        } else if (NODE_TYPE(source_node) == NT_FUNDEC) {
            source_header = FUNDEC_HEADER(source_node);
        }
        
        if (source_header != NULL) {
            char *new_full_name = FUNHEADER_NAME(source_header);
            // If the name has changed in the source, update the call node's name attribute
            if (!STReq(FUNCALL_NAME(node), new_full_name)) {
                MEMfree(FUNCALL_NAME(node));
                FUNCALL_NAME(node) = STRcpy(new_full_name);
                
                // Note: We do NOT update fun->name because it's a shared pointer from the table snapshot.
                // Updating it would either leak or cause double-free. 
                // Since this traversal runs at the end of Adjustment, FUNCALL_NAME(node) is what matters.
            }
        }
    }
    return node;
}
