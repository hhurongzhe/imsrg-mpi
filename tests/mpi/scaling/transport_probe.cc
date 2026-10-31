#include <mpi.h>
#include <vector>
#include <algorithm>
#include <iostream>
int main(int argc,char** argv) {
 MPI_Init(&argc,&argv);
 int rank,size; MPI_Comm_rank(MPI_COMM_WORLD,&rank); MPI_Comm_size(MPI_COMM_WORLD,&size);
 for(int mib:{8,64,256}) {
  std::vector<double> data((mib*1024UL*1024UL)/sizeof(double)); std::vector<double> times;
  for(int rep=0;rep<6;++rep) {
   int root=rep%size; if(rank==root)std::fill(data.begin(),data.end(),double(rep));
   MPI_Barrier(MPI_COMM_WORLD); double t=MPI_Wtime();
   MPI_Bcast(data.data(),data.size(),MPI_DOUBLE,root,MPI_COMM_WORLD); t=MPI_Wtime()-t;
   double worst; MPI_Allreduce(&t,&worst,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
   for(double x:data)if(x!=rep)MPI_Abort(MPI_COMM_WORLD,1);
   if(rep)times.push_back(worst);
  }
  std::sort(times.begin(),times.end());
  if(!rank)std::cout<<"BCAST MiB="<<mib<<" median_seconds="<<times[2]<<" MiB_per_second="<<mib/times[2]<<std::endl;
 }
 MPI_Finalize();
}
