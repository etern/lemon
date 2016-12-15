#ifndef _REPORT_H_
#define _REPORT_H_

void Reprint(struct lemon *);
void ReportOutput(struct lemon *);
void ReportTable(struct lemon *, int);
void ReportHeader(struct lemon *);
void CompressTables(struct lemon *);
void ResortStates(struct lemon *);

#endif // _REPORT_H_
