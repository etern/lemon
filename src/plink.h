#ifndef _PLINK_H_
#define _PLINK_H_

struct plink *Plink_new(void);
void Plink_add(struct plink **, struct config *);
void Plink_copy(struct plink **, struct plink *);
void Plink_delete(struct plink *);

#endif // _PLINK_H_
