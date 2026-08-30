#pragma once

typedef struct SemanticNode {

} SemanticNode;

typedef struct SemanticNodeList SemanticNodeList;
struct SemanticNodeList {
  SemanticNode *node;
  SemanticNodeList *next;
};