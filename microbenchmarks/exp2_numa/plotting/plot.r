library(ggplot2)
library(sqldf)
library(forcats)
library(ggrepel)
library("wesanderson")
library(ggthemes)
library(readr)
library(dplyr)
library(gridExtra)
library(knitr)
library(unikn)  # load package
options(scipen=999)


library(RColorBrewer)
# display.brewer.all(colorblindFriendly = TRUE)


theme =c("#7FA0C1", "#A2BE8A", "#B48CAD")
## theme =c("#00A08A", "#F98400", "#5BBCD6")


df = read.csv("/workspaces/thesis-encryptedDB/microbenchmarks/CounterTest/plotting/result.csv", header=TRUE, sep=",", row.names=NULL)


df = sqldf("
SELECT mode, shuffle, cross_numa, median
FROM df
")

{
    cross_numa_str <- "true"
    shuffle_str <- "true"
    #  WHERE cross_numa='%s' AND shuffle='%s'
    columns <- "mode, AVG(median) as median, cross_numa, shuffle"
    query <- sprintf("SELECT %s
     FROM df
     GROUP BY mode, cross_numa, shuffle", columns)
p1 <- ggplot(sqldf(query), aes(x = mode, y = median, fill = mode)) +
geom_col() +
labs(
    y = "Latency per Node [nsec]"
) +
facet_grid(shuffle ~ cross_numa)
theme_bw() 
# title=sprintf("cross_numa='%s' AND shuffle='%s", cross_numa_str, shuffle_str)
print(p1)
}

ggsave("/workspaces/thesis-encryptedDB/microbenchmarks/CounterTest/plotting/plots/numa_access.pdf",width=8, height=3)