// Run this from regression/integrate location
// Parquet files are hard to construct. We have resorted to creating json files and 
// using this scala script to convert it to parquet format.
// The script is run from a spark-shell.
// Entering paste mode (ctrl-D to finish)

val filename = "data/optional_parquet.json"
val df = spark.read
    .format("json")
    .load(filename)
df.write.mode(SaveMode.Overwrite).parquet("data/optional_parquet.parquet")
